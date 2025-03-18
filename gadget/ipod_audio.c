#define pr_fmt(fmt) "ipod-gadget-audio: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>
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



#define BUFFER_BYTES_MAX (PAGE_SIZE * 16)
#define PRD_SIZE_MAX PAGE_SIZE

#define MIN_PERIODS 4

#define NUM_USB_AUDIO_TRANSFERS 4
#define MAX_USB_AUDIO_PACKET_SIZE 180

#include "ipod.h"

struct ipod_audio {
	struct usb_function func;
	int ac_intf, ac_alt;
	int as_intf, as_alt;
	

	struct platform_device *pdev;
	struct snd_card *card;
	struct snd_pcm *pcm;

	struct snd_pcm_substream *ss;

	size_t dma_bytes;
	unsigned char *dma_area;
	size_t period_size;

	ssize_t hw_ptr;
	void *rbuf;

	spinlock_t play_lock;

	struct usb_ep *in_ep;
	bool in_ep_enabled;
	struct usb_request **in_req;

	int cnt;
};

static inline struct ipod_audio *func_to_ipod_audio(struct usb_function *f)
{
	return container_of(f, struct ipod_audio, func);
}

static struct snd_pcm_hardware ipod_audio_hw = {
	.info = SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME,
	.rates = SNDRV_PCM_RATE_44100,
	.rate_min = 44100,
	.rate_max = 44100,
	.buffer_bytes_max = BUFFER_BYTES_MAX,
	//.period_bytes_max = PRD_SIZE_MAX,
	//.period_bytes_min = MAX_USB_AUDIO_PACKET_SIZE,
	.period_bytes_min = 180 / 2,
	.period_bytes_max = PRD_SIZE_MAX,
	.periods_min = MIN_PERIODS,
	.periods_max = BUFFER_BYTES_MAX / PRD_SIZE_MAX,
	.channels_min = 2,
	.channels_max = 2,
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
};

static int ipod_audio_pcm_open(struct snd_pcm_substream *substream)
{
	struct ipod_audio *audio = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	runtime->hw = ipod_audio_hw;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
	{
		spin_lock_init(&audio->play_lock);
		audio->cnt = 0;
	}

	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

	return 0;
}

static int ipod_audio_pcm_close(struct snd_pcm_substream *substream)
{
	struct ipod_audio *audio = snd_pcm_substream_chip(substream);
	return 0;
}

static int ipod_audio_pcm_hw_params(struct snd_pcm_substream *substream,
									struct snd_pcm_hw_params *hw_params)
{
	struct ipod_audio *audio = snd_pcm_substream_chip(substream);
	int err = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
	{
		err = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
		if (err >= 0)
		{
			audio->dma_bytes = substream->runtime->dma_bytes;
			audio->dma_area = substream->runtime->dma_area;
			audio->period_size = params_period_bytes(hw_params);
		}
	}
	return err;
}

static int ipod_audio_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct ipod_audio *audio = snd_pcm_substream_chip(substream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
	{
		audio->dma_bytes = 0;
		audio->dma_area = NULL;
		audio->period_size = 0;
	}

	return snd_pcm_lib_free_pages(substream);
}

static int ipod_audio_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct ipod_audio *audio = snd_pcm_substream_chip(substream);
	unsigned long flags;
	int err = 0;

	spin_lock_irqsave(&audio->play_lock, flags);

	/* Reset */
	audio->hw_ptr = 0;

	switch (cmd)
	{
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		audio->ss = substream;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		audio->ss = NULL;
		break;
	default:
		err = -EINVAL;
	}

	spin_unlock_irqrestore(&audio->play_lock, flags);

	/* Clear buffer after Play stops */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK && !audio->ss)
		memset(audio->rbuf, 0, MAX_USB_AUDIO_PACKET_SIZE * NUM_USB_AUDIO_TRANSFERS);

	return err;
}

static snd_pcm_uframes_t ipod_audio_pcm_hw_pointer(struct snd_pcm_substream *substream)
{
	struct ipod_audio *audio = snd_pcm_substream_chip(substream);
	return bytes_to_frames(substream->runtime, audio->hw_ptr);
}

static int ipod_audio_pcm_null(struct snd_pcm_substream *substream)
{
	struct ipod_audio *audio = snd_pcm_substream_chip(substream);
	return 0;
}

static struct snd_pcm_ops ipod_audio_pcm_ops = {
	.open = ipod_audio_pcm_open,
	.close = ipod_audio_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = ipod_audio_pcm_hw_params,
	.hw_free = ipod_audio_pcm_hw_free,
	.trigger = ipod_audio_pcm_trigger,
	.pointer = ipod_audio_pcm_hw_pointer,
	.prepare = ipod_audio_pcm_null,
};

static void ipod_audio_iso_complete(struct usb_ep *ep, struct usb_request *req)
{
	unsigned pending;
	unsigned long flags;
	unsigned int hw_ptr;
	bool update_alsa = false;
	struct snd_pcm_substream *substream;
	struct ipod_audio *audio = req->context;
	int ret;

	//trace_printk("status=%d ep_enabled=%d\n", req->status, audio->in_ep_enabled);
	//trace_ipod_req_out_done(req);

	//if (req->status == -ESHUTDOWN)
	if (!audio->in_ep_enabled || req->status)
		return;
	
	// if(req->status) {
	// 	usb_ep_free_request(audio->in_ep, req);
	// 	return;
	// }

	//if (audio->dma_area == NULL)
	//	goto exit;

	substream = audio->ss;

	if (!substream)
		goto exit;

	spin_lock_irqsave(&audio->play_lock, flags);

	if (audio->cnt < 9)
	{
		req->length = 176;
		req->actual = 176;
	}
	else
	{
		req->length = 180;
		req->actual = 180;
	}
	audio->cnt = (audio->cnt + 1) % 10;

	pending = audio->hw_ptr % audio->period_size;
	pending += req->actual;
	if (pending >= audio->period_size)
		update_alsa = true;

	hw_ptr = audio->hw_ptr;
	audio->hw_ptr = (audio->hw_ptr + req->actual) % audio->dma_bytes;

	spin_unlock_irqrestore(&audio->play_lock, flags);

	/* Pack USB load in ALSA ring buffer */
	pending = audio->dma_bytes - hw_ptr;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
	{
		if (unlikely(pending < req->actual))
		{
			//printk("Oops: %d / %d \n", pending, req->actual);
			memcpy(req->buf, audio->dma_area + hw_ptr, pending);
			memcpy(req->buf + pending, audio->dma_area, req->actual - pending);
		}
		else
		{
			memcpy(req->buf, audio->dma_area + hw_ptr, req->actual);
		}
	}

exit:

	//req->length = MAX_USB_AUDIO_PACKET_SIZE;
	//req->actual = req->length;
	ret = usb_ep_queue(audio->in_ep, req, GFP_ATOMIC);
	if (ret) {
		trace_printk("queue: err=%d\n", ret);
	}

	if (update_alsa)
		snd_pcm_period_elapsed(substream);

	return;
}



int ipod_audio_setup(struct usb_function *func, const struct usb_ctrlrequest *ctrl)
{
	struct usb_composite_dev *cdev = func->config->cdev;
	struct usb_request *req = cdev->req;

	u16 w_index = le16_to_cpu(ctrl->wIndex);
	u16 w_value = le16_to_cpu(ctrl->wValue);
	u16 w_length = le16_to_cpu(ctrl->wLength);
	int status = 0;
	DBG(func->config->cdev, " = %s() \n", __FUNCTION__);
	DBG(cdev,
		"Control req: %02x.%02x v%04x i%04x l%d\n",
		ctrl->bRequestType, ctrl->bRequest,
		w_value, w_index, w_length);

	switch (ctrl->bRequest) {
	case UAC_SET_CUR:
		req->zero = 0;
		req->length = w_length;
		status = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (status < 0)
		{
			ERROR(cdev, "usb_ep_queue error on ep0 %d\n", status);
			return status;
		}

		return status;
		break;
	case UAC_GET_CUR:
	case UAC_GET_MIN:
	case UAC_GET_MAX:
	case UAC_GET_RES:
		req->zero = 0;
		req->length = w_length;
		status = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (status < 0)
			ERROR(cdev, "usb_ep_queue error on ep0 %d\n", status);
		return status;
		break;
	}

	return -EOPNOTSUPP;
}

int ipod_audio_start(struct ipod_audio* audio) {
	int ret = 0;
	int i;
	struct usb_request *req;

	pr_info("audio start\n");

	if(audio->in_ep_enabled) {
		return 0;
	}
	audio->in_ep_enabled = true;
	ret = config_ep_by_speed(audio->func.config->cdev->gadget, &audio->func, audio->in_ep);
	if (ret)
	{
		DBG(audio->func.config->cdev, "config_ep_by_speed FAILED!\n");
		return ret;
	}
	
	ret = usb_ep_enable(audio->in_ep);
	if (ret < 0)
	{
		DBG(audio->func.config->cdev, "Enable IN endpoint FAILED!\n");
		return ret;
	}

	usb_ep_fifo_flush(audio->in_ep);
	

	
	for (i = 0; i < NUM_USB_AUDIO_TRANSFERS; i++){
		if (!audio->in_req[i]) {
			req = usb_ep_alloc_request(audio->in_ep, GFP_ATOMIC);
			if(req == NULL) {
				return -ENOMEM;
			}
			audio->in_req[i] = req;
			req->zero = 0;
			req->context = audio;
			req->length = MAX_USB_AUDIO_PACKET_SIZE;
			req->complete = ipod_audio_iso_complete;
			req->buf = audio->rbuf + i * MAX_USB_AUDIO_PACKET_SIZE;
		}

		if (usb_ep_queue(audio->in_ep, audio->in_req[i], GFP_ATOMIC)){
			ERROR(audio->func.config->cdev, "usb_ep_queue error on ep0\n");
		}
	}
	
	return 0;
}

int ipod_audio_stop(struct ipod_audio* audio) {
	int i;
	pr_info("audio stop\n");
	if(!audio->in_ep_enabled) {
		return 0;
	}
	audio->in_ep_enabled = false;
	for (i = 0; i < NUM_USB_AUDIO_TRANSFERS; i++) {
		if(audio->in_req[i]) {
			usb_ep_dequeue(audio->in_ep, audio->in_req[i]);
			usb_ep_free_request(audio->in_ep, audio->in_req[i]);
			audio->in_req[i] = NULL;
		}
	}
				
	usb_ep_disable(audio->in_ep);

	return 0;
}

int ipod_audio_set_alt(struct usb_function *func, unsigned intf, unsigned alt)
{
	struct ipod_audio *audio = func_to_ipod_audio(func);
	DBG(func->config->cdev, " = %s(%u,%u) \n", __FUNCTION__, intf, alt);

	if (intf == audio->ac_intf) {
		if (alt > 0) {
			ERROR(func->config->cdev, "%s:%d Error!\n", __func__, __LINE__);
			return -EINVAL;
		}
		return 0;
	}

	if (intf == audio->as_intf) {
		audio->as_alt = alt;
		switch(alt) {
		case 0:
			return ipod_audio_stop(audio);
		case 1:
			return ipod_audio_start(audio);
		default:
			return -EINVAL;
		}
	}

	return -EINVAL;
}

int ipod_audio_get_alt(struct usb_function *func, unsigned intf)
{
	struct ipod_audio *audio = func_to_ipod_audio(func);
	DBG(func->config->cdev, " = %s() \n", __FUNCTION__);

	if (intf == audio->ac_intf) {
		return audio->ac_alt;
	}

	if (intf == audio->as_intf) {
		return audio->as_alt;
	}
	
	return -EINVAL;
}

void ipod_audio_disable(struct usb_function *func)
{
	struct ipod_audio *audio = func_to_ipod_audio(func);
	DBG(func->config->cdev, " = %s() \n", __FUNCTION__);
	audio->as_alt = 0;
	ipod_audio_stop(audio);
	
}


void ipod_audio_suspend(struct usb_function *func)
{
	struct ipod_audio *audio = func_to_ipod_audio(func);
	DBG(func->config->cdev, " = %s() \n", __FUNCTION__);
	
}

void ipod_audio_resume(struct usb_function *func)
{
	struct ipod_audio *audio = func_to_ipod_audio(func);
	DBG(func->config->cdev, " = %s() \n", __FUNCTION__);
	
}

int ipod_audio_bind(struct usb_configuration *conf, struct usb_function *func)
{
	int ret = 0;
	int i;
	struct ipod_audio *audio = func_to_ipod_audio(func);
	DBG(conf->cdev, " = %s() \n", __FUNCTION__);

	audio->ac_intf = usb_interface_id(conf, func);
	audio->ac_alt = 0;
	if(audio->ac_intf < 0) {
		return audio->ac_intf;
	}
	audio->as_intf = usb_interface_id(conf, func);
	audio->as_alt = 0;
	if(audio->as_intf < 0) {
		return audio->as_intf;
	}

	ipod_audio_control_desc.bInterfaceNumber = audio->ac_intf;
	ipod_audio_stream_0_desc.bInterfaceNumber = audio->as_intf;
	ipod_audio_stream_1_desc.bInterfaceNumber = audio->as_intf;

	audio->in_ep = usb_ep_autoconfig(conf->cdev->gadget, &ipod_audio_stream_1_endpoint_fs);
	if (!audio->in_ep) {
		return -ENODEV;
	}
	ipod_audio_stream_1_endpoint_hs.bEndpointAddress = ipod_audio_stream_1_endpoint_fs.bEndpointAddress;

	#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)
	ret = usb_assign_descriptors(func, ipod_audio_desc_fs, ipod_audio_desc_hs, NULL, NULL);
	#else
	ret = usb_assign_descriptors(func, ipod_audio_desc_fs, ipod_audio_desc_hs, NULL);
	#endif

	if(ret) {
		return ret;
	}

	audio->rbuf = kzalloc(MAX_USB_AUDIO_PACKET_SIZE * NUM_USB_AUDIO_TRANSFERS, GFP_KERNEL);
	audio->in_req = kzalloc(NUM_USB_AUDIO_TRANSFERS * sizeof(struct usb_request *), GFP_KERNEL);
	for (i = 0; i < NUM_USB_AUDIO_TRANSFERS; i++)
	{
		audio->in_req[i] = NULL;
	}

	//AUDIO CARD
	audio->pdev = platform_device_alloc("snd_usb_ipod", -1);
	if (IS_ERR(audio->pdev))
	{
		ret = PTR_ERR(audio->pdev);
		DBG(conf->cdev, "Coudn't create platform device: %d", ret);
		return ret;
	}

	ret = platform_device_add(audio->pdev);
	if (ret)
	{
		DBG(conf->cdev, "Coudn't add platform device: %d", ret);
		goto pdev_fail;
	}

	#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,15,0)
	ret = snd_card_new(&audio->pdev->dev, -1, "iPod USB", THIS_MODULE, 0, &audio->card);
	#else
	ret = snd_card_create(-1, "iPod USB", THIS_MODULE, 0, &audio->card);
	#endif

	if (ret)
	{
		DBG(conf->cdev, "Coudn't create audio card: %d", ret);
		goto pdev_fail;
	}

	ret = snd_pcm_new(audio->card, "iPod PCM", 0, 1, 0, &audio->pcm);
	if (ret)
	{
		DBG(conf->cdev, "Coudn't create audio device: %d", ret);
		goto snd_fail;
	}
	audio->pcm->private_data = audio;

	snd_pcm_set_ops(audio->pcm, SNDRV_PCM_STREAM_PLAYBACK, &ipod_audio_pcm_ops);
	snd_pcm_lib_preallocate_pages_for_all(audio->pcm, SNDRV_DMA_TYPE_CONTINUOUS, snd_dma_continuous_data(GFP_KERNEL), 0, BUFFER_BYTES_MAX);
	ret = snd_card_register(audio->card);
	if (ret)
	{
		DBG(conf->cdev, "Coudn't register audio card: %d", ret);
		goto snd_fail;
	}

	return 0;

snd_fail:
	snd_card_free(audio->card);
	audio->card = NULL;
	audio->pcm = NULL;
pdev_fail:
	platform_device_del(audio->pdev);
	audio->pdev = NULL;
	return ret;
}

void ipod_audio_unbind(struct usb_configuration *conf, struct usb_function *func)
{
	int i;
	struct ipod_audio *audio = func_to_ipod_audio(func);
	DBG(conf->cdev, " = %s() \n", __FUNCTION__);

	if (audio->card != NULL)
	{
		snd_card_free(audio->card);
		platform_device_del(audio->pdev);
		audio->card = NULL;
		audio->pcm = NULL;
		audio->pdev = NULL;
	}

	for (i = 0; i < NUM_USB_AUDIO_TRANSFERS; i++)
	{
		if (audio->in_req[i] != NULL)
		{
			usb_ep_free_request(audio->in_ep, audio->in_req[i]);
			audio->in_req[i] = NULL;
		}
	}

	usb_ep_disable(audio->in_ep);
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
	usb_ep_autoconfig_release(audio->in_ep);
	#endif
	audio->in_ep = NULL;
	kfree(audio->rbuf);
}

static void ipod_audio_free(struct usb_function *func) 
{
	struct ipod_audio *audio = func_to_ipod_audio(func);
	kfree(audio);
}

static struct usb_function *ipod_audio_alloc(struct usb_function_instance *fi)
{
	struct ipod_audio *audio;

	audio = kzalloc(sizeof(*audio), GFP_KERNEL);
	if (!audio)
		return ERR_PTR(-ENOMEM);


	audio->func.name = "ipod_audio";
	audio->func.bind = ipod_audio_bind;
	audio->func.unbind = ipod_audio_unbind;
	audio->func.set_alt = ipod_audio_set_alt;
	audio->func.get_alt = ipod_audio_get_alt;
	audio->func.setup = ipod_audio_setup;
	audio->func.disable = ipod_audio_disable;
	audio->func.suspend = ipod_audio_suspend;
	audio->func.resume = ipod_audio_resume;
	audio->func.free_func = ipod_audio_free;

	return &audio->func;
}

static void ipod_audio_free_inst(struct usb_function_instance *fi)
{
	kfree(fi);
}

static void ipod_attr_release(struct config_item *item)
{
	struct usb_function_instance *fi;
	fi = container_of(to_config_group(item), struct usb_function_instance, group);
	usb_put_function_instance(fi);
}

static struct configfs_item_operations ipod_item_ops = {
	.release	= ipod_attr_release,
};

static struct config_item_type ipod_audio_func_type = {
	.ct_owner	 = THIS_MODULE,
	.ct_item_ops = &ipod_item_ops,
};

static struct usb_function_instance *ipod_audio_alloc_inst(void)
{
	struct usb_function_instance *fi;
	fi = kzalloc(sizeof(*fi), GFP_KERNEL);
	if (!fi)
		return ERR_PTR(-ENOMEM);
	
	fi->free_func_inst = ipod_audio_free_inst;
	config_group_init_type_name(&fi->group, "", &ipod_audio_func_type);
	return fi;
}

DECLARE_USB_FUNCTION_INIT(ipod_audio, ipod_audio_alloc_inst, ipod_audio_alloc);

MODULE_AUTHOR("Andrew Onyshchuk");
MODULE_LICENSE("GPL");
