// static char *fn_play = FILE_PCM_PLAYBACK;
// module_param(fn_play, charp, S_IRUGO);
// MODULE_PARM_DESC(fn_play, "Playback PCM device file name");

#define IPOD_USB_VENDOR 0x05ac
#define IPOD_USB_PRODUCT 0x12a8

static struct usb_string strings_dev[] = {
	[USB_GADGET_MANUFACTURER_IDX].s = "",
	[USB_GADGET_PRODUCT_IDX].s = "",
	[USB_GADGET_SERIAL_IDX].s = "",
	{  } /* end of list */
};

static struct usb_gadget_strings stringtab_dev = {
	.language = 0x0409,	/* en-us */
	.strings = strings_dev,
};

static struct usb_gadget_strings *ipod_strings[] = {
	&stringtab_dev,
	NULL,
};


static struct usb_device_descriptor device_desc = {
	.bLength =		sizeof device_desc,
	.bDescriptorType =	USB_DT_DEVICE,
	.bcdUSB =		cpu_to_le16(0x200),
	.bDeviceClass =		0,
	.bDeviceSubClass =	0,
	.bDeviceProtocol =	0,
	/* .bMaxPacketSize0 = f(hardware) */
	.idVendor =		cpu_to_le16(IPOD_USB_VENDOR),
	.idProduct =		cpu_to_le16(IPOD_USB_PRODUCT),
	.bcdDevice = cpu_to_le16(0x0401),
	/* .iManufacturer = DYNAMIC */
	/* .iProduct = DYNAMIC */
	/* NO SERIAL NUMBER */
	.bNumConfigurations =	2,
};

//static const struct usb_descriptor_header *otg_desc[2];


// ===== IPOD function
static struct usb_interface_descriptor ipod_audio_control_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting =	0,
	.bNumEndpoints =	0,
	.bInterfaceClass =	1,
	.bInterfaceSubClass =	USB_SUBCLASS_AUDIOCONTROL,
	/* .iInterface		= DYNAMIC */
};

static struct uac1_ac_header_descriptor ipod_audio_control_uac_header = {
	.bLength =            9,
	.bDescriptorType=    36,
	.bDescriptorSubtype= 1,
	.bcdADC=             cpu_to_le16(0x0100),
	.bInCollection =      1,
	.baInterfaceNr = {1},
	.wTotalLength=       cpu_to_le16(30)
};

static struct uac_input_terminal_descriptor ipod_audio_control_uac_input_terminal = {
	.bLength=            12,
	.bDescriptorType=    36,
	.bDescriptorSubtype= 2,
	.bTerminalID=        1,
	.wTerminalType=      cpu_to_le16(0x0201),
	.bAssocTerminal=     2,
	.bNrChannels=        2,
	.wChannelConfig=     cpu_to_le16(0x0003),
	.iChannelNames=      0,
	.iTerminal=          0
};

static struct uac1_output_terminal_descriptor ipod_audio_control_uac_output_terminal = {
	.bLength=            9,
	.bDescriptorType=    36,
	.bDescriptorSubtype= 3,
	.bTerminalID=        2,
	.wTerminalType=      cpu_to_le16(0x0101),
	.bAssocTerminal=     1,
	.bSourceID =        1,
	.iTerminal =          0
};

static struct usb_interface_descriptor ipod_audio_stream_0_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting =	0,
	.bNumEndpoints =	0,
	.bInterfaceClass =	1,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOSTREAMING
};

static struct usb_interface_descriptor ipod_audio_stream_1_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting =	1,
	.bNumEndpoints =	1,
	.bInterfaceClass =	1,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOSTREAMING
};

static struct uac1_as_header_descriptor ipod_audio_stream_1_uac_header = {
	.bLength =		UAC_DT_AS_HEADER_SIZE,
	.bDescriptorType =	36 ,
	.bDescriptorSubtype =  UAC_FORMAT_TYPE_I_PCM ,
	.bTerminalLink =	2,
	.bDelay =	1,
	.wFormatTag =	cpu_to_le16(1)
};



static DECLARE_UAC_FORMAT_TYPE_I_DISCRETE_DESC(9) ipod_audio_stream_1_uac_discrete = {
	.bLength =		35,
	.bDescriptorType =	36 ,
	.bDescriptorSubtype =  2 ,
	.bFormatType =	1,
	.bNrChannels =	2,
	.bSubframeSize =	2,
	.bBitResolution =	16,
	.bSamFreqType =	9,
	.tSamFreq =	{{0x40, 0x1F, 0x00}, {0x11, 0x2B, 0x00}, {0xE0, 0x2E, 0x00}, {0x80, 0x3E, 0x00}, {0x22, 0x56, 0x00}, {0xC0, 0x5D, 0x00}, {0x00, 0x7D, 0x00}, {0x44, 0xAC, 0x00}, {0x80, 0xBB, 0x00}}
};

static struct usb_endpoint_descriptor ipod_audio_stream_1_endpoint_fs = {
	.bLength =		USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT  ,
	.bEndpointAddress =  1 | USB_DIR_IN ,
	.bmAttributes =	USB_ENDPOINT_XFER_ISOC | USB_ENDPOINT_SYNC_NONE,
	.wMaxPacketSize =	cpu_to_le16(192),
	.bInterval =	1,
	.bRefresh =	0,
	.bSynchAddress = 0
};

static struct usb_endpoint_descriptor ipod_audio_stream_1_endpoint_hs = {
	.bLength =		USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT  ,
	.bEndpointAddress =  1 | USB_DIR_IN ,
	.bmAttributes =	USB_ENDPOINT_XFER_ISOC | USB_ENDPOINT_SYNC_NONE,
	.wMaxPacketSize =	cpu_to_le16(192),
	.bInterval =	4,
	.bRefresh =	0,
	.bSynchAddress = 0
};

static struct uac_iso_endpoint_descriptor ipod_audio_stream_1_endpoint_uac = {
	.bLength =		UAC_ISO_ENDPOINT_DESC_SIZE,
	.bDescriptorType =	37  ,
	.bDescriptorSubtype =  1 ,
	.bmAttributes =	1,
	.bLockDelayUnits =	0,
	.wLockDelay =	cpu_to_le16(0x0)
};



static struct usb_descriptor_header *ipod_audio_desc_fs[] = {
	(struct usb_descriptor_header *) &ipod_audio_control_desc,
	(struct usb_descriptor_header *) &ipod_audio_control_uac_header,
	(struct usb_descriptor_header *) &ipod_audio_control_uac_input_terminal,
	(struct usb_descriptor_header *) &ipod_audio_control_uac_output_terminal,

	(struct usb_descriptor_header *) &ipod_audio_stream_0_desc,
	(struct usb_descriptor_header *) &ipod_audio_stream_1_desc,
	(struct usb_descriptor_header *) &ipod_audio_stream_1_uac_header,
	(struct usb_descriptor_header *) &ipod_audio_stream_1_uac_discrete,
	(struct usb_descriptor_header *) &ipod_audio_stream_1_endpoint_fs,
	(struct usb_descriptor_header *) &ipod_audio_stream_1_endpoint_uac,
	NULL
};

static struct usb_descriptor_header *ipod_audio_desc_hs[] = {
	(struct usb_descriptor_header *) &ipod_audio_control_desc,
	(struct usb_descriptor_header *) &ipod_audio_control_uac_header,
	(struct usb_descriptor_header *) &ipod_audio_control_uac_input_terminal,
	(struct usb_descriptor_header *) &ipod_audio_control_uac_output_terminal,

	(struct usb_descriptor_header *) &ipod_audio_stream_0_desc,
	(struct usb_descriptor_header *) &ipod_audio_stream_1_desc,
	(struct usb_descriptor_header *) &ipod_audio_stream_1_uac_header,
	(struct usb_descriptor_header *) &ipod_audio_stream_1_uac_discrete,
	(struct usb_descriptor_header *) &ipod_audio_stream_1_endpoint_hs,
	(struct usb_descriptor_header *) &ipod_audio_stream_1_endpoint_uac,
	NULL
};


// ==== HID


static struct usb_interface_descriptor ipod_hid_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bInterfaceNumber = 2,
	.bAlternateSetting =	0,
	.bNumEndpoints =	1,
	.bInterfaceClass =	3,
	.bInterfaceSubClass =	0,
	/* .iInterface		= DYNAMIC */
};



static struct hid_descriptor ipod_hid_desc2 = {
	.bLength =            9,
	.bDescriptorType=    33,
	.bcdHID= cpu_to_le16(0x0111),
	.bCountryCode=             0,
	.bNumDescriptors =      1,
	.desc = {{
		.bDescriptorType = 34,
		.wDescriptorLength = cpu_to_le16(208)
	}
	}
};



static struct usb_endpoint_descriptor ipod_hid_endpoint = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT  ,
	.bEndpointAddress =  3 | USB_DIR_IN ,
	.bmAttributes =	USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(64),
	.bInterval =	1,
};

static unsigned char ipod_hid_report[] = {
  0x06, 0x00, 0xff, 0x09, 0x01, 0xa1, 0x01, 0x75, 0x08, 0x26, 0x80, 0x00,
  0x15, 0x00, 0x09, 0x01, 0x85, 0x01, 0x95, 0x05, 0x82, 0x02, 0x01, 0x09,
  0x01, 0x85, 0x02, 0x95, 0x09, 0x82, 0x02, 0x01, 0x09, 0x01, 0x85, 0x03,
  0x95, 0x0d, 0x82, 0x02, 0x01, 0x09, 0x01, 0x85, 0x04, 0x95, 0x11, 0x82,
  0x02, 0x01, 0x09, 0x01, 0x85, 0x05, 0x95, 0x19, 0x82, 0x02, 0x01, 0x09,
  0x01, 0x85, 0x06, 0x95, 0x31, 0x82, 0x02, 0x01, 0x09, 0x01, 0x85, 0x07,
  0x95, 0x5f, 0x82, 0x02, 0x01, 0x09, 0x01, 0x85, 0x08, 0x95, 0xc1, 0x82,
  0x02, 0x01, 0x09, 0x01, 0x85, 0x09, 0x96, 0x01, 0x01, 0x82, 0x02, 0x01,
  0x09, 0x01, 0x85, 0x0a, 0x96, 0x81, 0x01, 0x82, 0x02, 0x01, 0x09, 0x01,
  0x85, 0x0b, 0x96, 0x01, 0x02, 0x82, 0x02, 0x01, 0x09, 0x01, 0x85, 0x0c,
  0x96, 0xff, 0x02, 0x82, 0x02, 0x01, 0x09, 0x01, 0x85, 0x0d, 0x95, 0x05,
  0x92, 0x02, 0x01, 0x09, 0x01, 0x85, 0x0e, 0x95, 0x09, 0x92, 0x02, 0x01,
  0x09, 0x01, 0x85, 0x0f, 0x95, 0x0d, 0x92, 0x02, 0x01, 0x09, 0x01, 0x85,
  0x10, 0x95, 0x11, 0x92, 0x02, 0x01, 0x09, 0x01, 0x85, 0x11, 0x95, 0x19,
  0x92, 0x02, 0x01, 0x09, 0x01, 0x85, 0x12, 0x95, 0x31, 0x92, 0x02, 0x01,
  0x09, 0x01, 0x85, 0x13, 0x95, 0x5f, 0x92, 0x02, 0x01, 0x09, 0x01, 0x85,
  0x14, 0x95, 0xc1, 0x92, 0x02, 0x01, 0x09, 0x01, 0x85, 0x15, 0x95, 0xff,
  0x92, 0x02, 0x01, 0xc0
};



static struct usb_descriptor_header *ipod_hid_desc_fs_hs[] = {
	(struct usb_descriptor_header *) &ipod_hid_desc,
	(struct usb_descriptor_header *) &ipod_hid_desc2,
	(struct usb_descriptor_header *) &ipod_hid_endpoint,

	NULL
};
