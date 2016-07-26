#ifndef USBAPI_H
#define USBAPI_H

#include "global.h"
#include "usbview.h"


BEGIN_EXTERN_C

typedef struct usbapi_device usbapi_device;

struct usbapi_device_info{
    /** Platform-specific device path */
    char *path;
    /** Device Vendor ID */
    unsigned short vendor_id;
    /** Device Product ID */
    unsigned short product_id;
    /** Serial Number */
    char *serial_number;
    /** Device Release Number in binary-coded decimal,
        also known as Device Version Number */
    unsigned short release_number;
    /** Manufacturer String */
    char *manufacturer_string;
    /** Product string */
    char *product_string;
    /** Usage Page for this Device/Interface
        (Windows/Mac only). */
    unsigned short usage_page;
    /** Usage for this Device/Interface
        (Windows/Mac only).*/
    unsigned short usage;
    /** The USB interface which this logical device
        represents. Valid on both Linux implementations
        in all cases, and valid on the Windows implementation
        only if the device contains more than one interface. */
    int interface_number;
    /** (Linux only).*/
    int busnum;
    /** (Linux only).*/
    int devnum;
    /** USB-IF class code for the device. See \ref libusb_class_code. */
    enum usb_class_code class_code;
    /* Endpoint information */
    struct usbapi_device_endpoint *input_endpoint;
    struct usbapi_device_endpoint *output_endpoint;
    /** Pointer to the next device */
    struct usbapi_device_info *next;
};

typedef struct usbapi_device_info usbapi_device_info;

EXPORT usbapi_device_info *usbapi_enumerate(unsigned short vendor_id, unsigned short product_id);
EXPORT void usbapi_free_enumeration(usbapi_device_info *devs);
EXPORT usbapi_device_info* dup_usbapi_info(usbapi_device_info *dev_info);

EXPORT usbapi_device *  usbapi_open(usbapi_device_info *dev_info);
EXPORT usbapi_device *  usbapi_open_vid_pid(unsigned short vendor_id, unsigned short product_id);
EXPORT usbapi_device *  usbapi_open_vid_pid_class(unsigned short vendor_id, unsigned short product_id,enum usb_class_code class_code);
EXPORT int usbapi_isOpen(usbapi_device* dev);
EXPORT void usbapi_close(usbapi_device *dev);
EXPORT int  usbapi_write(usbapi_device* dev,const char* data,size_t length);
EXPORT int usbapi_read_timeout(usbapi_device *dev, char *data, size_t max, int msecs);
EXPORT int  usbapi_read(usbapi_device *dev, char *data, size_t max);
EXPORT void usbapi_flush(usbapi_device *dev);
EXPORT int  usbapi_pollin(usbapi_device *dev,int msecs);
EXPORT int  usbapi_pollout(usbapi_device *dev,int msecs);
EXPORT const usbapi_device_info *usbapi_getinfo(usbapi_device*dev);
EXPORT HANDLE usbapi_fd(usbapi_device *dev);


END_EXTERN_C

#endif // USBAPI_H
