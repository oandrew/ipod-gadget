#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/usb/hid.h>
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
    struct cdev cdev;
    // struct device *device;

    wait_queue_head_t waitq;
    STRUCT_KFIFO_REC_2(REPORT_LENGTH*4) read_fifo;
    struct work_struct send_work;
    struct completion  send_completion;

};

static inline struct ipod_hid *func_to_ipod_hid(struct usb_function *f)
{
    return container_of(f, struct ipod HID, func);
}

static ssize_t ipod_hid_dev_read(struct file *file, char __user *buffer,
                                 size_t count, loff_t *ptr)
{
    struct ipod_hid *hid = file->private_data;
    int ret = -EINVAL;

    trace_printk("len=%zu\n", count);

    ret = ipod_mutex_lock(&hid->lock, file->f_flags & O_NONBLOCK);
    if(ret) {
        return ret;
    }

    if (kfifo_is_empty(&hid->read_fifo)) {
        if (file->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto unlock;
        }
        ret = wait_event_interruptible(hid->waitq,
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

static ssize_t ipod_hid_dev_write(struct file *file, const char __user *buffer, size_t count, loff_t *offp)
{
    struct ipod_hid *hid = file->private_data;
    int ret = -EINVAL;

    trace_printk("len=%zu\n", count);

    ret = ipod_mutex_lock(&hid->lock, file->f_flags & O_NONBLOCK);
    if(ret) {
        return ret;
    }

    if (kfifo_avail(&hid->write_fifo) < count) {
        if (file->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto unlock;
        }
        ret = wait_event_interruptible(hid->waitq,
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
        pr_err("send buffer full!\n");
        ret = -EFAULT;
        goto unlock;
    }

    ret = count;
    trace_printk("fifo len: %d\n", kfifo_len(&hid->write_fifo));

    mutex_unlock(&hid->lock);
    return ret;
}

static unsigned int ipod_hid_dev_poll(struct file *file, poll_table *wait) {
    struct ipod_hid *hid = file->private_data;
    unsigned int ret = 0;

    trace_printk("read:%d write:%d\n", !!(ret & POLLIN), !!(ret & POLLOUT));

    return ret;
}

static void ipod_hid_recv_complete(struct usb_ep *ep, struct usb_request *req)
{
    struct ipod_hid *hid = req->context;
    int copied;

    trace_printk("len=%d actual=%d \n", req->length, req->actual);
    copied = kfifo_in(&hid->read_fifo, req->buf, req->length);

    if(unlikely(copied != req->length)) {
        pr_err("recv buffer full!\n");
        return;
    }

    wake_up_interruptible(&hid->waitq);

}

static ssize_t ipod_hid_send_complete(struct usb_ep *ep, struct usb_request *req)
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
        hid->in_req->status = 0;
        hid->in_req->zero = 0;
        hid->in_req->length = len;
        hid->in_req->context = hid;
        hid->in_req->complete = ipod_hid_send_complete;

        ret = usb_ep_queue(hid->in_ep, hid->in_req, GFP_ATOMIC);
        if(ret) {
            pr_err("usb_ep_queue error=%d\n", ret);
            continue;
        }

        wait_for_completion(&hid->send_completion);

        wake_up_interruptible(&hid->waitq);
    }
    trace_printk("done\n");
}

static ssize_t ipod_hid_dev_open(struct inode *inode, struct file *fd)
{
    struct ipod_hid *hid = 
        container_of(inode->i_cdev, struct ipod_hid_opts, fi);

    int ret;

    if(atomic_inc_return(&hid->refcnt) == 1) {
        pr_info("activating \n");
        if(hid->bound) {
            ret = usb_function_activate(&hid->func);
            if(ret) {
                pr_err("activating err=%d \n", ret);
                return ret;
            }
        }
        
    }

    return 0;
}

static int ipod_hid_dev_release(struct inode *inode, struct file *fd)
{
    struct ipod_hid *hid = fd->private_data;
    pr_info("ipod_hid_dev_release()\n");

    if(atomic_dec_and_test(&hid->refcnt))
    {
        pr_info("deactivating=%d \n", hid->bound);
        if(hid->bound) {
            ret = usb_function_deactivate(&hid->func);
            if(ret) {
                pr_err("deactivating err=%d \n", ret);
            }
        }

    }
    return 0;
}

static const struct file_operations ipod_hid_dev_ops = {
	.owner = THIS_MODULE,
	.open = ipod_hid_dev_open,
	.release = ipod_hid_dev_release,
	.read = ipod_hid_dev_read,
	.poll = ipod_hid_dev_poll,
};

static void __init ipod_hid_mod_init(void)
{
    struct usb_function *func;
    int ret;

    func = &ipod_hidusb_func;

    ret = usb_function_register(func);

    if(ret) {
        return;
    }

    ipod_hid_class = class_create(THIS_MODULE, "iap");
    if(IS_ERR(ipod_hid_class)) {
        goto exit;
    }

    return;

exit:
    cdev_init(&ipod_hid_cdev, &ipod_hid_dev_ops);
}

static void __exit ipod_hid_mod_exit(void)
{
    struct usb_function *func;
    int ret;

    func = &ipod_hidusb_func;

    ret = usb_function_unregister(func);

    if(ret) {
        return;
    }

    cdev_del(&ipod_hid_cdev, &ipod_hid_dev_ops);
}

module_init(ipod_hid_mod_init);
module_exit(ipod_hid_mod_exit);

MODULE_AUTHOR("Andrew Onyshchuk");
MODULE_LICENSE("GPL");
