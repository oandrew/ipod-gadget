#define pr_fmt(fmt) "ipod-gadget: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb/composite.h>
#include <linux/usb/audio.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/hid.h>
#include "ipod.h"



static bool only_ipod = false;
module_param(only_ipod, bool, 0);
MODULE_PARM_DESC(only_ipod, "Only ipod config");

static bool disable_audio = false;
module_param(disable_audio, bool, 0);
MODULE_PARM_DESC(disable_audio, "No audio intf");

static bool swap_configs = false;
module_param(swap_configs, bool, 0);
MODULE_PARM_DESC(swap_configs, "Present iPod USB config as #1");

static ushort product_id = 0;
module_param(product_id, ushort, 0);
MODULE_PARM_DESC(product_id, "Override USB Product ID");

static struct usb_function_instance *fi_ms;
static struct usb_function *f_ms;

static int ipod_config_ptp_bind(struct usb_configuration *conf)
{
	int err;
	DBG(conf->cdev, " = %s() \n", __FUNCTION__);

	f_ms = usb_get_function(fi_ms);
	if(IS_ERR(f_ms)) {
		return PTR_ERR(f_ms);
	}
	err = usb_add_function(conf, f_ms);
	if(err < 0) {
		usb_put_function(f_ms);
		return err;
	}

	return 0;
}

static void ipod_config_ptp_unbind(struct usb_configuration *conf)
{
	DBG(conf->cdev, " = %s() \n", __FUNCTION__);
	if (!IS_ERR_OR_NULL(f_ms)) {
		usb_put_function(f_ms);
	}
}
static int ipod_config_ptp_setup(struct usb_configuration *conf, const struct usb_ctrlrequest *ctrl)
{
	DBG(conf->cdev, " = %s() \n", __FUNCTION__);
	return 0;
}

static struct usb_configuration ipod_fake_ptp = {
	.label = "PTP",
	.bConfigurationValue = 1,
	/* .iConfiguration = DYNAMIC */
	.bmAttributes = USB_CONFIG_ATT_SELFPOWER,
	.MaxPower = 500,
	.unbind = ipod_config_ptp_unbind,
	.setup = ipod_config_ptp_setup,
};

// usb config
static struct usb_function_instance *ipod_hid_fi;
static struct usb_function *ipod_hid_f;

static struct usb_function_instance *ipod_audio_fi;
static struct usb_function *ipod_audio_f;

static int ipod_config_bind(struct usb_configuration *conf)
{
	
	DBG(conf->cdev, " = %s() \n", __FUNCTION__);
	if (!disable_audio) {
		usb_add_function(conf, ipod_audio_f);
	}
	usb_add_function(conf, ipod_hid_f);
	return 0;
}

static void ipod_config_unbind(struct usb_configuration *conf)
{
	DBG(conf->cdev, " = %s() \n", __FUNCTION__);
}
static int ipod_config_setup(struct usb_configuration *conf, const struct usb_ctrlrequest *ctrl)
{
	DBG(conf->cdev, " = %s() \n", __FUNCTION__);
	return 0;
}

static struct usb_configuration ipod_configuration = {
	.label = "iPod interface",
	.bConfigurationValue = 2,
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

	fi_ms = usb_get_function_instance("mass_storage");
	if(IS_ERR(fi_ms)) {
		return PTR_ERR(fi_ms);
	}

	if(!only_ipod) {
		usb_add_config(cdev, &ipod_fake_ptp, ipod_config_ptp_bind);
	}


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

	usb_put_function_instance(fi_ms);


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
	.max_speed = USB_SPEED_FULL,

	.bind = ipod_bind,
	.unbind = ipod_unbind,
	.disconnect = ipod_disconnect,

	.suspend = ipod_suspend,
	.resume = ipod_resume,

};



static int __init ipod_init(void)
{
	pr_info("init\n");

	if(swap_configs) {
		ipod_configuration.bConfigurationValue = 1;
		ipod_fake_ptp.bConfigurationValue = 2;
		pr_info("swapping usb configuration: ipod is #1\n");
	}

	if(product_id != 0) {
		device_desc.idProduct = cpu_to_le16(product_id);
		pr_info("override usb idProduct: %04x\n", product_id);
	}

	return usb_composite_probe(&ipod_driver);
	
}

static void __exit ipod_exit(void)
{
	pr_info("exit\n");
	usb_composite_unregister(&ipod_driver);
}

module_init(ipod_init);
module_exit(ipod_exit);

MODULE_AUTHOR("Andrew Onyshchuk");
MODULE_LICENSE("GPL");
