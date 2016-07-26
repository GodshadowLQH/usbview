#include "../../src/usbview.h"
#include <stdio.h>

#define LOG(fmt,...)          do{fprintf(stdout,fmt"\n",##__VA_ARGS__);}while(0)


int main(int argc,char** argv)
{
    (void)argc;
    (void)argv;

    usb_device_info* info = get_usb_devices();

    LOG("This is information from my usb-devices:");
    while(info){
        int i,j,k;
        LOG("T:  Bus=%02x Lev=%02x Prnt=%02x Port=%02x Cnt=%02x Dev#=%3d Spd=%d  Mxch=%2d",
            info->busnum,info->level,info->parent?info->parent->devnum:0,info->portNumber,
            info->connectorNumber,info->devnum,info->speed,info->maxchild);
        if(info->bandwidth)
            LOG("B:  Alloc=%3d/%3d us (%2d%%), #Int=%3d, #Iso=%3d",
                info->bandwidth->allocated,info->bandwidth->total,
                info->bandwidth->allocated*100/info->bandwidth->total,
                info->bandwidth->numInterruptRequests,info->bandwidth->numIsocRequests);
        LOG("D:  Ver= %s Cls=%02x(%-5s) Sub=%02x Prot=%02x MxPs=%d #Cfgs=%3d",
            info->version?info->version:"",
            info->bDeviceClass,parse_usb_class_code(info->bDeviceClass),
            info->bDeviceSubClass,info->bDeviceProtocol,
            info->bMaxPacketSize0,info->bNumConfigurations);
        LOG("P:  Vendor=%04x ProdID=%04x Rev= %s",
            info->idVendor,info->idProduct,info->bcdDevice?info->bcdDevice:"");
        if(info->manufacturer)
            LOG("S:  Manufacturer=%s",info->manufacturer);
        if(info->product)
            LOG("S:  Product=%s",info->product);
        if(info->serial)
            LOG("S:  SerialNumber=%s",info->serial);
        for(i=0;i<info->bNumConfigurations;++i){
            LOG("C:* #Ifs=%2d Cfg#=%2d Atr=%02x MxPwr=%3dmA",
                info->config[i]->bNumInterfaces,info->config[i]->bConfigurationValue,
                info->config[i]->bmAttributes,info->config[i]->bMaxPower);
            for(j=0;j<info->config[i]->bNumInterfaces;++j){
                LOG("I:* If#=%2d Alt=%2d #EPs=%2d Cls=%02x(%-5s) Sub=%02x Prot=%02x Driver=%s",
                    info->config[i]->interfaces[j]->bInterfaceNumber,
                    info->config[i]->interfaces[j]->bAlternateSetting,
                    info->config[i]->interfaces[j]->bNumEndpoints,
                    info->config[i]->interfaces[j]->bInterfaceClass,
                    parse_usb_class_code(info->config[i]->interfaces[j]->bInterfaceClass),
                    info->config[i]->interfaces[j]->bInterfaceSubClass,
                    info->config[i]->interfaces[j]->bInterfaceProtocol,
                    info->config[i]->interfaces[j]->driver);
                for(k=0;k<info->config[i]->interfaces[j]->bNumEndpoints;++k){
                    LOG("E:  Ad=%02x(%s) Atr=%02x(%s) MxPs=%4d Ivl=%dms",
                        info->config[i]->interfaces[j]->endpoint[k]->bEndpointAddress,
                        info->config[i]->interfaces[j]->endpoint[k]->in==USB_ENDPOINT_IN?"I":"O",
                        info->config[i]->interfaces[j]->endpoint[k]->bmAttributes,
                        parse_usb_transfer_type(info->config[i]->interfaces[j]->endpoint[k]->bmAttributes),
                        info->config[i]->interfaces[j]->endpoint[k]->wMaxPacketSize,
                        info->config[i]->interfaces[j]->endpoint[k]->bInterval);
                }
            }
        }

        LOG("");

        info = info->next;
    }

    free_usb_devices(info);


    return 0;
}
