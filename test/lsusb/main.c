#include "../../src/usbview.h"
#include <stdio.h>


#define LOG(fmt,...)          do{fprintf(stdout,fmt"\n",##__VA_ARGS__);}while(0)

int main(int argc,char** argv)
{
    (void)argc;
    (void)argv;

    usb_device_info* info = get_usb_devices();

    LOG("This is information from my lsusb:");
    while(info){
        LOG("Bus %03x Device %03x: ID %04x:%04x",
            info->busnum,info->devnum,info->idVendor,info->idProduct);
        info = info->next;
    }

    free_usb_devices(info);

    return 0;
}
