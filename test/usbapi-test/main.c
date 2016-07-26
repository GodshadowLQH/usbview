#include "../../src/usbapi.h"

#define LOG(fmt,...)          do{fprintf(stdout,fmt"\n",##__VA_ARGS__);}while(0)

#define VENDER_ID   0x5114
#define PRODUCT_ID  0x4830

int main(int argc,char** argv)
{
    usbapi_device * dev = NULL;
    int readed = -1;
    char buf[128];
    (void)argc;
    (void)argv;

    dev = usbapi_open_vid_pid(VENDER_ID,PRODUCT_ID);
    if(!dev){
        LOG("open failed!");
        return -1;
    }

    if(usbapi_write(dev,"1234",4)<=0){
        LOG("write failed!");
        return -1;
    }

    readed = usbapi_read_timeout(dev,buf,sizeof(buf),5000);
    if(readed<=0){
        LOG("read failed!");
        return -1;
    }

    LOG("Read %d bytes!",readed);

    return 0;
}
