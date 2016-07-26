#ifndef USBVIEW_C
#define USBVIEW_C

#ifdef __cplusplus
extern "C"{
#endif



/** \ingroup desc
 * Device and/or Interface Class codes */
enum usb_class_code {
    /** In the context of a \ref libusb_device_descriptor "device descriptor",
     * this bDeviceClass value indicates that each interface specifies its
     * own class information and all interfaces operate independently.
     */
    USB_CLASS_PER_INTERFACE = 0,

    /** Audio class */
    USB_CLASS_AUDIO = 1,

    /** Communications class */
    USB_CLASS_COMM = 2,

    /** Human Interface Device class */
    USB_CLASS_HID = 3,

    /** Physical */
    USB_CLASS_PHYSICAL = 5,

    /** Image class */
    USB_CLASS_IMAGE = 6,

    /** Printer class */
    USB_CLASS_PRINTER = 7,

    /** Mass storage class */
    USB_CLASS_MASS_STORAGE = 8,

    /** Hub class */
    USB_CLASS_HUB = 9,

    /** Data class */
    USB_CLASS_DATA = 0x0A,

    /** Smart Card */
    USB_CLASS_SMART_CARD = 0x0b,

    /** Content Security */
    USB_CLASS_CONTENT_SECURITY = 0x0d,

    /** Video */
    USB_CLASS_VIDEO = 0x0e,

    /** Personal Healthcare */
    USB_CLASS_PERSONAL_HEALTHCARE = 0x0f,

    /** Diagnostic Device */
    USB_CLASS_DIAGNOSTIC_DEVICE = 0xdc,

    /** Wireless class */
    USB_CLASS_WIRELESS = 0xe0,

    /** Application class */
    USB_CLASS_APPLICATION = 0xfe,

    /** Class is vendor-specific */
    USB_CLASS_VENDOR_SPEC = 0xff
};

/** \ingroup desc
 * Endpoint direction. Values for bit 7 of the
 * \ref libusb_endpoint_descriptor::bEndpointAddress "endpoint address" scheme.
 */
enum usb_endpoint_direction {
    /** In: device-to-host */
    USB_ENDPOINT_IN = 0x80,

    /** Out: host-to-device */
    USB_ENDPOINT_OUT = 0x00
};

/** \ingroup desc
 * Endpoint transfer type. Values for bits 0:1 of the
 * \ref libusb_endpoint_descriptor::bmAttributes "endpoint attributes" field.
 */
enum usb_transfer_type {
    /** Control endpoint */
    USB_TRANSFER_TYPE_CONTROL = 0,

    /** Isochronous endpoint */
    USB_TRANSFER_TYPE_ISOCHRONOUS = 1,

    /** Bulk endpoint */
    USB_TRANSFER_TYPE_BULK = 2,

    /** Interrupt endpoint */
    USB_TRANSFER_TYPE_INTERRUPT = 3,

    /** Stream endpoint */
    USB_TRANSFER_TYPE_BULK_STREAM = 4
};

typedef struct usb_device_endpoint {
    enum usb_endpoint_direction	in;
    int		bEndpointAddress;
    int		bmAttributes;
    int		wMaxPacketSize;
    int		bInterval;   // mseconds
} usb_device_endpoint;


typedef struct usb_device_interface {
    char	*driver;
    char    *path;
    int		bInterfaceNumber;
    int		bAlternateSetting;
    int		bNumEndpoints;
    int     bInterfaceClass;
    int		bInterfaceSubClass;
    int		bInterfaceProtocol;
    usb_device_endpoint	**endpoint;
    int     attached;
} usb_device_interface;



typedef struct usb_device_config {
    int		bConfigurationValue;
    int		bNumInterfaces;
    int		bmAttributes;
    int 	bMaxPower;   // mA
    usb_device_interface	**interfaces;
} usb_device_config;


typedef struct usb_device_bandwidth {
    int		allocated;
    int		total;
    int		numInterruptRequests;
    int		numIsocRequests;
} usb_device_bandwidth;


typedef struct usb_device_info {
    char	*name;

    int		busnum;
    int		level;
    int		portNumber;
    int		connectorNumber;
    int		devnum;
    int		speed;

    char	*version;
    int     bDeviceClass;
    int     bDeviceSubClass;
    int     bDeviceProtocol;
    int		bMaxPacketSize0;

    int		idVendor;
    int		idProduct;
    char	*bcdDevice;

    char	*manufacturer;
    char	*product;
    char	*serial;

    int		bNumConfigurations;
    usb_device_config	**config;

    // for usb device except hub
    struct usb_device_info	*parent;

    // for usb hub
    int		maxchild;
    struct usb_device_info **children;
    usb_device_bandwidth	*bandwidth;

    struct usb_device_info* next;
} usb_device_info;

extern usb_device_info* get_usb_devices();
extern void free_usb_devices(usb_device_info*);
extern const char* parse_usb_class_code(int class_code);
extern const char* parse_usb_transfer_type(int transfer_type);

#ifdef __cplusplus
}
#endif

#endif // USBVIEW_C
