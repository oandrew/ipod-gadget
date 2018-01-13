#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb/composite.h>
#include <linux/usb/audio.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/hid.h>
#include "ipod.h"

// usb config
static struct usb_function_instance *ipod_hid_fi;
static struct usb_function *ipod_hid_f;

static struct usb_function_instance *ipod_audio_fi;
static struct usb_function *ipod_audio_f;

int ipod_config_bind(struct usb_configuration *conf)
{
	
	DBG(conf->cdev, " = %s() \n", __FUNCTION__);
	usb_add_function(conf, ipod_audio_f);
	usb_add_function(conf, ipod_hid_f);
	return 0;
}

void ipod_config_unbind(struct usb_configuration *conf)
{
	DBG(conf->cdev, " = %s() \n", __FUNCTION__);
}
int ipod_config_setup(struct usb_configuration *conf, const struct usb_ctrlrequest *ctrl)
{
	DBG(conf->cdev, " = %s() \n", __FUNCTION__);
	return 0;
}

static struct usb_configuration ipod_configuration = {
	.label = "iPod interface",
	.bConfigurationValue = 1,
	/* .iConfiguration = DYNAMIC */
	.bmAttributes = USB_CONFIG_ATT_SELFPOWER,
	.MaxPower = 500,
	.unbind = ipod_config_unbind,
	.setup = ipod_config_setup,
};


// driver
static int ipod_bind(struct usb_composite_dev *cdev)
{
	int ret = 0;
	DBG(cdev, " = %s() \n", __FUNCTION__);

	ipod_audio_fi = usb_get_function_instance("ipod_audio");
    if(IS_ERR(ipod_audio_fi)) {
        return PTR_ERR(ipod_audio_fi);
    }

	ipod_audio_f = usb_get_function(ipod_audio_fi);
	if(IS_ERR(ipod_audio_f)) {
		return PTR_ERR(ipod_audio_f);
	}


	ipod_hid_fi = usb_get_function_instance("ipod_hid");
    if(IS_ERR(ipod_hid_fi)) {
        return PTR_ERR(ipod_hid_fi);
    }

	ipod_hid_f = usb_get_function(ipod_hid_fi);
	if(IS_ERR(ipod_hid_f)) {
		return PTR_ERR(ipod_hid_f);
	}

	usb_add_config(cdev, &ipod_configuration, ipod_config_bind);
	return ret;
}

static int ipod_unbind(struct usb_composite_dev *cdev)
{
	DBG(cdev, " = %s() \n", __FUNCTION__);

	usb_put_function(ipod_audio_f);
	usb_put_function_instance(ipod_audio_fi);

	usb_put_function(ipod_hid_f);
	usb_put_function_instance(ipod_hid_fi);
	return 0;
}

static void ipod_disconnect(struct usb_composite_dev *cdev)
{
	DBG(cdev, " = %s() \n", __FUNCTION__);
}

static void ipod_suspend(struct usb_composite_dev *cdev)
{
	DBG(cdev, " = %s() \n", __FUNCTION__);
}

static void ipod_resume(struct usb_composite_dev *cdev)
{
	DBG(cdev, " = %s() \n", __FUNCTION__);
}

static struct usb_composite_driver ipod_driver = {
	.name = "g_ipod",
	.dev = &device_desc,
	.strings = ipod_strings,
	.max_speed = USB_SPEED_HIGH,

	.bind = ipod_bind,
	.unbind = ipod_unbind,
	.disconnect = ipod_disconnect,

	.suspend = ipod_suspend,
	.resume = ipod_resume,

};



module_driver(ipod_driver, usb_composite_probe, usb_composite_unregister);

MODULE_AUTHOR("Andrew Onyshchuk");
MODULE_LICENSE("GPL");
