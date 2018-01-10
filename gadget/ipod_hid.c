#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb/composite.h>
#include <linux/usb/audio.h>
#include <linux/usb/ch9.h>
#include <linux/hid.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/poll.h>
#include <linux/kfifo.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <linux/platform_device.h>

#include "ipod.h"
//#include "ipod_hid.h"

#define REPORT_LENGTH 1024

struct class *ipod_hid_class;

struct ipod_hid
{
	struct usb_function func;

	atomic_t refcnt;
	bool bound;

	struct mutex lock;

	int intf;
	struct usb_ep *in_ep;

	//char device
	// dev_t dev_id;
	// struct class *class_id;
	int major;
	struct cdev cdev;
	// struct device *device;

	

	// recv
	STRUCT_KFIFO_REC_2(REPORT_LENGTH*4) read_fifo;
	wait_queue_head_t read_waitq;
	//spinlock_t read_lock;

	// send
	struct usb_request *in_req;

	STRUCT_KFIFO_REC_2(REPORT_LENGTH*4) write_fifo;
	wait_queue_head_t write_waitq;
	struct work_struct send_work;
	struct completion  send_completion;

};

static inline struct ipod_hid *func_to_ipod_hid(struct usb_function *f)
{
	return container_of(f, struct ipod_hid, func);
}

static int ipod_mutex_lock(struct mutex *mutex, unsigned nonblock)
{
	return nonblock
		? likely(mutex_trylock(mutex)) ? 0 : -EAGAIN
		: mutex_lock_interruptible(mutex);
}

static void ipod_hid_recv_complete(struct usb_ep *ep, struct usb_request *req)
{
    struct ipod_hid *hid = req->context;
	int copied;
	trace_printk("len=%d actual=%d \n", req->length, req->actual);
	copied = kfifo_in(&hid->read_fifo, req->buf, req->length);
	if(unlikely(copied != req->length)) {
		pr_err("ipod-gadget-hid: recv buffer full!\n");
		return;
	}
	wake_up_interruptible(&hid->read_waitq);
}


static ssize_t ipod_hid_dev_read(struct file *file, char __user *buffer,
								 size_t count, loff_t *ptr)
{
    struct ipod_hid *hid = file->private_data;
	int ret = -EINVAL;
	int n, copied;

	if (!count)
		return 0;

	trace_printk("len=%lu\n", count);

	ret = ipod_mutex_lock(&hid->lock, file->f_flags & O_NONBLOCK);
	if(ret) {
		return ret;
	}
	

	if (kfifo_is_empty(&hid->read_fifo)) {
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto unlock;
		}
		ret = wait_event_interruptible(hid->read_waitq,
			!kfifo_is_empty(&hid->read_fifo));

		if(ret) {
			goto unlock;
		}
	}

	n = kfifo_peek_len(&hid->read_fifo);
	if(count < n) {
		ret = -EFAULT;
		goto unlock;
	}

	ret = kfifo_to_user(&hid->read_fifo, buffer, count, &copied);
	if(ret) {
		goto unlock;
	}
	if(WARN_ON(copied != n)) {
		ret = -EFAULT;
		goto unlock;
	}
	ret = copied;

unlock:
	mutex_unlock(&hid->lock);
	return ret;
}

// Sent a report
static void ipod_hid_send_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct ipod_hid *hid = req->context;
    complete(&hid->send_completion);
}

static void ipod_hid_send_workfn(struct work_struct *work) {
	struct ipod_hid* hid = container_of(work, struct ipod_hid, send_work);
	int ret;
	int len;
	trace_printk("started\n");
	while((len = kfifo_out(&hid->write_fifo,hid->in_req->buf, REPORT_LENGTH)) > 0) {
		trace_printk("send len=%d\n", len);
		//msleep(1000);
		
		reinit_completion(&hid->send_completion);

		hid->in_req->status = 0;
		hid->in_req->zero = 0;
		hid->in_req->length = len;
		hid->in_req->context = hid;
		hid->in_req->complete = ipod_hid_send_complete;

		ret = usb_ep_queue(hid->in_ep, hid->in_req, GFP_ATOMIC);
		if(ret) {
			pr_err("ipod-gadget-hid: usb_ep_queue error=%d\n", ret);
			continue;
		}

		wait_for_completion(&hid->send_completion);

		wake_up_interruptible(&hid->write_waitq);
	}
	trace_printk("done\n");
}



static ssize_t ipod_hid_dev_write(struct file *file, const char __user *buffer, size_t count, loff_t *offp)
{
	struct ipod_hid *hid = file->private_data;
    int ret;
	int copied;
	bool was_scheduled;

	trace_printk("len=%lu\n", count);

	ret = ipod_mutex_lock(&hid->lock, file->f_flags & O_NONBLOCK);
	if(ret) {
		return ret;
	}

	if (kfifo_avail(&hid->write_fifo) < count) {
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto unlock;
		}
		ret = wait_event_interruptible(hid->write_waitq,
			kfifo_avail(&hid->write_fifo) >= count);
		if(ret) {
			goto unlock;
		}
	}

	ret = kfifo_from_user(&hid->write_fifo, buffer, count, &copied);
	if(ret) {
		goto unlock;
	}
	if(copied != count) {
		pr_err("ipod-gadget-hid: send buffer full!\n");
		ret = -EFAULT;
		goto unlock;
	}

	ret = count;
	trace_printk("fifo len: %d\n", kfifo_len(&hid->write_fifo));

	was_scheduled = schedule_work(&hid->send_work);
	trace_printk("schedule=%d\n", was_scheduled);

unlock:
	mutex_unlock(&hid->lock);
	return ret;
}

static unsigned int ipod_hid_dev_poll(struct file *file, poll_table *wait) {
    struct ipod_hid *hid = file->private_data;
	unsigned int ret = 0;
	pr_info("ipod-gadget-hid: ipod_hid_dev_poll()\n");

	poll_wait(file, &hid->read_waitq, wait);
	poll_wait(file, &hid->write_waitq, wait);

	if (!kfifo_is_empty(&hid->read_fifo))
		ret |= POLLIN | POLLRDNORM;

	if (kfifo_avail(&hid->write_fifo))
		ret |= POLLOUT | POLLWRNORM;

	return ret;
}






static int ipod_hid_dev_open(struct inode *inode, struct file *fd) {
	int ret;
	struct ipod_hid *hid = 
		container_of(inode->i_cdev, struct ipod_hid, cdev);
	pr_info("ipod-gadget-hid: ipod_hid_dev_open()\n");

	fd->private_data = hid;

	if(atomic_inc_return(&hid->refcnt) == 1) {
		pr_info("ipod-gadget-hid: activating \n");
		if(hid->bound) {
			ret = usb_function_activate(&hid->func);
			if(ret) {
				pr_err("ipod-gadget-hid: activating err=%d \n", ret);
				return ret;
			}
		}
		
	}
	

	return 0;
}

static int ipod_hid_dev_release(struct inode *inode, struct file *fd) {
	int ret;
	struct ipod_hid *hid = fd->private_data;
	pr_info("ipod-gadget-hid: ipod_hid_dev_release()\n");

	if(atomic_dec_and_test(&hid->refcnt))
	{
		pr_info("ipod-gadget-hid: deactivating=%d \n", hid->bound);
		if(hid->bound) {
			ret = usb_function_deactivate(&hid->func);
			if(ret) {
				pr_err("ipod-gadget-hid: deactivating err=%d \n", ret);
			}
		}
		

	}
	return 0;
}

static const struct file_operations ipod_hid_dev_ops = {
	.owner = THIS_MODULE,
	.open = ipod_hid_dev_open,
	.release = ipod_hid_dev_release,
	.write = ipod_hid_dev_write,
	.read = ipod_hid_dev_read,
	.poll = ipod_hid_dev_poll,
};

// usb
static bool ipod_hid_req_match(struct usb_function *func,const struct usb_ctrlrequest *ctrl,bool config0) {
    switch(ctrl->bRequest) {
    case 0x40:
    	return true;
  	}
    return false;
}

int ipod_hid_setup(struct usb_function *func, const struct usb_ctrlrequest *ctrl)
{
    struct ipod_hid *hid = func_to_ipod_hid(func);
	struct usb_composite_dev *cdev = func->config->cdev;
	struct usb_request *req = cdev->req;

	u16 w_index = le16_to_cpu(ctrl->wIndex);
	u16 w_value = le16_to_cpu(ctrl->wValue);
	u16 w_length = le16_to_cpu(ctrl->wLength);
	int status = 0;
	int length = w_length;

	DBG(func->config->cdev, " = %s() \n", __FUNCTION__);
	DBG(cdev,
		"Control req: %02x.%02x v%04x i%04x l%d\n",
		ctrl->bRequestType, ctrl->bRequest,
		w_value, w_index, w_length);

	switch (ctrl->bRequest)
	{
	case USB_REQ_GET_DESCRIPTOR:
		VDBG(cdev, "get hid descriptor\n");
		memcpy(req->buf, ipod_hid_report, 208);
		goto respond;
		break;
	case HID_REQ_SET_REPORT:
		req->complete = ipod_hid_recv_complete;
        req->context = hid;
		goto respond;
		break;
	case HID_REQ_SET_IDLE:
		VDBG(cdev, "set idle \n");
		length = 0;
		goto respond;
		break;
	case 0x40:
		VDBG(cdev, "apple vendor 0x40 \n");
		goto respond;
		break;
	default:
		VDBG(cdev, "unknown request! \n");
	}

//stall:
	return -EOPNOTSUPP;

respond:
	req->zero = 0;
	req->length = length;
	status = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
	if (status < 0)
		ERROR(cdev, "usb_ep_queue error on ep0 %d\n", status);
	return status;
}

int ipod_hid_set_alt(struct usb_function *func, unsigned intf, unsigned alt)
{
    struct ipod_hid *hid = func_to_ipod_hid(func);
	int ret = 0;

	DBG(func->config->cdev, " = %s() \n", __FUNCTION__);
    if (intf == hid->intf) {
		if (alt > 0) {
			ERROR(func->config->cdev, "%s:%d Error!\n", __func__, __LINE__);
			return -EINVAL;
		}
		ret = usb_ep_disable(hid->in_ep);
		if(ret) {
			DBG(func->config->cdev, "usb_ep_disable FAILED!\n");
			return ret;
		}

		ret = config_ep_by_speed(func->config->cdev->gadget, &hid->func, hid->in_ep);
		if (ret)
		{
			DBG(func->config->cdev, "config_ep_by_speed FAILED!\n");
			return ret;
		}

		ret = usb_ep_enable(hid->in_ep);
		if (ret < 0)
		{
			DBG(func->config->cdev, "Enable IN endpoint FAILED!\n");
			return ret;
		}

		return 0;
	}

	return -EINVAL;
}

void ipod_hid_disable(struct usb_function *func)
{
    struct ipod_hid *hid = func_to_ipod_hid(func);
	DBG(func->config->cdev, " = %s() \n", __FUNCTION__);

	usb_ep_disable(hid->in_ep);
}

int ipod_hid_bind(struct usb_configuration *conf, struct usb_function *func)
{
    struct ipod_hid *hid = func_to_ipod_hid(func);
	int ret = 0;

	usb_function_deactivate(func);

	DBG(conf->cdev, " = %s(), deactivs=%d \n", __FUNCTION__, conf->cdev->deactivations);
	hid->intf = usb_interface_id(conf, func);

	ipod_hid_desc.bInterfaceNumber = hid->intf;

	//usb stuff
	hid->in_ep = usb_ep_autoconfig(conf->cdev->gadget, &ipod_hid_endpoint);
	if (!hid->in_ep) {
		ERROR(conf->cdev, "usb_ep_autoconfig FAILED\n");
		return -ENODEV;
	}

	ret = usb_assign_descriptors(func, ipod_hid_desc_fs_hs, ipod_hid_desc_fs_hs, NULL, NULL);
	if(ret) {
        return ret;
    }

	hid->in_req = usb_ep_alloc_request(hid->in_ep, GFP_KERNEL);
	hid->in_req->buf = kmalloc(REPORT_LENGTH, GFP_KERNEL);

	
	

	
	if(atomic_read(&hid->refcnt) > 0) {
		usb_function_activate(&hid->func);
	}
	hid->bound = true;

	printk("ret=%d\n", ret);
	return ret;
}

void ipod_hid_unbind(struct usb_configuration *conf, struct usb_function *func)
{
    struct ipod_hid *hid = func_to_ipod_hid(func);
	DBG(conf->cdev, " = %s(), deactivs=%d\n", __FUNCTION__, conf->cdev->deactivations);
	hid->bound = false;

	

	if(atomic_read(&hid->refcnt) > 0) {
		usb_function_deactivate(&hid->func);
	}

	kfree(hid->in_req->buf);
	usb_ep_free_request(hid->in_ep, hid->in_req);
	usb_ep_autoconfig_release(hid->in_ep);

	hid->in_ep = NULL;


	usb_function_activate(func);

	
}

// function



struct ipod_hid_opts {
	struct usb_function_instance	fi;
	dev_t dev;
};


static void ipod_hid_free(struct usb_function *func) 
{
    struct ipod_hid *hid = func_to_ipod_hid(func);
	pr_info("ipod-gadget-hid: ipod_hid_free()\n");

	device_destroy(ipod_hid_class, MKDEV(hid->major, 0));
	cdev_del(&hid->cdev);

    kfree(hid);
}

static struct usb_function *ipod_hid_alloc(struct usb_function_instance *fi)
{
	int ret;
	dev_t dev;
	struct device *device;
	
	struct ipod_hid_opts *opts 
		= container_of(fi, struct ipod_hid_opts, fi);

	struct ipod_hid *hid = kzalloc(sizeof(*hid), GFP_KERNEL);
	if (!hid)
		return ERR_PTR(-ENOMEM);
	pr_info("ipod-gadget-hid: ipod_hid_alloc()\n");

	

    hid->func.name = "ipod_hid";
    hid->func.bind = ipod_hid_bind;
    hid->func.unbind = ipod_hid_unbind;
    hid->func.set_alt = ipod_hid_set_alt;
    hid->func.setup = ipod_hid_setup;
    hid->func.disable = ipod_hid_disable;
    hid->func.free_func = ipod_hid_free;
    hid->func.req_match = ipod_hid_req_match;
	hid->major = MAJOR(opts->dev);
	hid->func.bind_deactivated = false;

	mutex_init(&hid->lock);
	atomic_set(&hid->refcnt, 0);

	INIT_KFIFO(hid->read_fifo);
	init_waitqueue_head(&hid->read_waitq);

	init_waitqueue_head(&hid->write_waitq);
	INIT_KFIFO(hid->write_fifo);
	INIT_WORK(&hid->send_work, ipod_hid_send_workfn);
	init_completion(&hid->send_completion);	

	

	//char device
	cdev_init(&hid->cdev, &ipod_hid_dev_ops);
	dev = MKDEV(hid->major, 0);
	if((ret = cdev_add(&hid->cdev, dev, 1))) {
		printk("cdev_add err=%d\n", ret);
	}

	device = device_create(ipod_hid_class, NULL, 
		dev, NULL, "iap%d", MINOR(dev));
	if(IS_ERR(device)) {
		return PTR_ERR(device);
	}
	



	return &hid->func;
}



// ipod_hid instance
static void ipod_attr_release(struct config_item *item)
{
	struct ipod_hid_opts *opts 
		= container_of(to_config_group(item), struct ipod_hid_opts, fi.group);

	usb_put_function_instance(&opts->fi);
}

static struct configfs_item_operations ipod_item_ops = {
	.release	= ipod_attr_release,
};

static struct config_item_type ipod_hid_func_type = {
	.ct_owner	 = THIS_MODULE,
    .ct_item_ops = &ipod_item_ops,
};

static void ipod_hid_free_inst(struct usb_function_instance *fi)
{
	struct ipod_hid_opts *opts 
		= container_of(fi, struct ipod_hid_opts, fi);
	
	unregister_chrdev_region(opts->dev, 1);
	kfree(opts);
}

static struct usb_function_instance *ipod_hid_alloc_inst(void)
{
	int err;
	struct ipod_hid_opts *opts;
	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);

	if((err = alloc_chrdev_region(&opts->dev, 0, 1, "iap"))) {
		printk("alloc_chrdev_region err=%d\n", err);
	}

	printk("alloc_chrdev_region dev: %d %d\n", 
		MAJOR(opts->dev), MINOR(opts->dev));
	

	opts->fi.free_func_inst = ipod_hid_free_inst;
	config_group_init_type_name(&opts->fi.group, "", &ipod_hid_func_type);
	

	return &opts->fi;
}

DECLARE_USB_FUNCTION(ipod_hid, ipod_hid_alloc_inst, ipod_hid_alloc);



static int __init ipod_hid_mod_init(void)
{
	ipod_hid_class = class_create(THIS_MODULE, "iap");
	if(IS_ERR(ipod_hid_class)) {
		return PTR_ERR(ipod_hid_class);
	}


	return usb_function_register(&ipod_hidusb_func);
}
static void __exit ipod_hid_mod_exit(void)
{
	class_destroy(ipod_hid_class);
	usb_function_unregister(&ipod_hidusb_func);
}

module_init(ipod_hid_mod_init);
module_exit(ipod_hid_mod_exit);

MODULE_AUTHOR("Andrew Onyshchuk");
MODULE_LICENSE("GPL");