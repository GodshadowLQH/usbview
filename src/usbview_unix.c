#include "usbview.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <errno.h>
#include <linux/kdev_t.h>
#include <linux/limits.h>
#include <dirent.h>
#include <stdarg.h>
#include <fnmatch.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef HAVE_CONFIG_H
    #include <../config.h>
#else
    #undef LIB_DEBUG
    #define PACKAGE_BUGREPORT ""
#endif

#if 1
#define USBVIEW_LOG(fmt,...)          fprintf(stdout,fmt"\n",##__VA_ARGS__)
#define USBVIEW_LOG_ERROR(fmt,...)    fprintf(stderr,fmt" in func:%s line:%d\n",##__VA_ARGS__,__FUNCTION__,__LINE__)
#else
#define USBVIEW_LOG(fmt,...)
#define USBVIEW_LOG_ERROR(fmt,...)
#endif

#define NEW(p,type) type* p;p = (type*)malloc(sizeof(type));memset(p,0,sizeof(type))

#define SYSFS_DEVICE_PATH   "/sys/bus/usb/devices"
#define SYSFS_DEV_PATH      "/dev"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
/* descriptor name */
#define TOPOLOGY_BUS_STRING             "Bus="
#define TOPOLOGY_LEVEL_STRING			"Lev="
#define TOPOLOGY_PARENT_STRING			"Prnt="
#define TOPOLOGY_PORT_STRING			"Port="
#define TOPOLOGY_COUNT_STRING			"Cnt="
#define TOPOLOGY_DEVICENUMBER_STRING	"Dev#="
#define TOPOLOGY_SPEED_STRING			"Spd="
#define TOPOLOGY_MAXCHILDREN_STRING		"MxCh="

#define BANDWIDTH_ALOCATED              "Alloc="
#define BANDWIDTH_TOTAL                 "/"
#define BANDWIDTH_PERCENT               "us ("
#define BANDWIDTH_INTERRUPT_TOTAL		"#Int="
#define BANDWIDTH_ISOC_TOTAL			"#Iso="

#define DEVICE_VERSION_STRING			"Ver="
#define DEVICE_CLASS_STRING             "Cls="
#define DEVICE_SUBCLASS_STRING			"Sub="
#define DEVICE_PROTOCOL_STRING			"Prot="
#define DEVICE_MAXPACKETSIZE_STRING		"MxPS="
#define DEVICE_NUMCONFIGS_STRING		"#Cfgs="
#define DEVICE_VENDOR_STRING			"Vendor="
#define DEVICE_PRODUCTID_STRING			"ProdID="
#define DEVICE_REVISION_STRING			"Rev="
#define DEVICE_MANUFACTURER_STRING		"Manufacturer="
#define DEVICE_PRODUCT_STRING			"Product="
#define DEVICE_SERIALNUMBER_STRING		"SerialNumber="

#define CONFIG_NUMINTERFACES_STRING		"#Ifs="
#define CONFIG_CONFIGNUMBER_STRING		"Cfg#="
#define CONFIG_ATTRIBUTES_STRING		"Atr="
#define CONFIG_MAXPOWER_STRING			"MxPwr="

#define INTERFACE_NUMBER_STRING			"If#="
#define INTERFACE_ALTERNATESETTING_STRING	"Alt="
#define INTERFACE_NUMENDPOINTS_STRING		"#EPs="
#define INTERFACE_CLASS_STRING			"Cls="
#define INTERFACE_SUBCLASS_STRING		"Sub="
#define INTERFACE_PROTOCOL_STRING		"Prot="
#define INTERFACE_DRIVERNAME_STRING		"Driver="
#define INTERFACE_DRIVERNAME_NODRIVER_STRING	"(none)"

#define ENDPOINT_ADDRESS_STRING			"Ad="
#define ENDPOINT_ATTRIBUTES_STRING		"Atr="
#define ENDPOINT_MAXPACKETSIZE_STRING	"MxPS="
#define ENDPOINT_INTERVAL_STRING		"Ivl="

static int GetInt (const char *string, char *pattern, int base)
{
    char    *pointer;

    pointer = strstr (string, pattern);
    if (pointer == NULL)
        return(0);

    pointer += strlen(pattern);
    return((int)(strtol (pointer, NULL, base)));
}


static char* GetString (const char *data,const char *pattern,int skip_sapce)
{
    char* p,*end,*ret = NULL;
    size_t maxlen,len;

    if(!data||!pattern)
        return NULL;

    if((p = strstr(data,pattern)) == NULL)
        return NULL;

    p += strlen(pattern);

    maxlen = strlen(data);
    if(skip_sapce){
        // skip space
        for(;(p)<(data+maxlen) && *p==' ';++p);
        if(p>=data+maxlen)
            return NULL;
        end = strchr(p,' ');
        if(end){
            len = (end-p);
        }else{
            // save all left
            len = maxlen-(p-data);
        }
        ret = (char*)malloc(len+1);
        memcpy(ret,p,len);
        ret[len] = 0;
    }else{
        len = maxlen-(p-data);
        ret = (char*)malloc(len+1);
        memcpy(ret,p,len);
        ret[len] = 0;
    }

    return ret;

}



static void DestroyInterface (usb_device_interface *interface)
{
    int i;
    if (interface == NULL)
        return;

    free (interface->driver);
    free (interface->path);
    for(i=0;i<interface->bNumEndpoints;++i)
        free (interface->endpoint[i]);
    free (interface->endpoint);
    free (interface);

    return;
}


static void DestroyConfig (usb_device_config *config)
{
    int     i;

    if (config == NULL)
        return;

    for (i = 0; i < config->bNumInterfaces; ++i)
        DestroyInterface (config->interfaces[i]);
    free (config);

    return;
}


static usb_device_info *usb_find_device (int devnum, int busnum,usb_device_info* root)
{
    usb_device_info  *ret = root;

    /* search with device_number and bus_number*/
    while(ret){
        if((ret->devnum == devnum) &&
                (ret->busnum == busnum))
            return ret;
        ret = ret->next;
    }
    return (NULL);
}

static void strRev(char* buf)
{
    char c;
    int len,i;
    if(!buf)
        return;

    len = strlen(buf);
    for(i=0;i<len/2;++i){
        c = buf[i];
        buf[i] = buf[len-i-1];
        buf[len-i-1] = c;
    }
}

static char *usb_deep_find_path(char* path,u_int8_t major,u_int8_t minor)
{
    char* ret = NULL;
    DIR *dir = NULL;
    struct dirent *dir_ptr = NULL;
    char buf[PATH_MAX];

    USBVIEW_LOG("deep find path with %d:%d ... in %s",major,minor,path);
    dir = opendir(path);
    if(!dir){
        USBVIEW_LOG("open %s failed!%s",path,strerror(errno));
        return NULL;
    }

    while((dir_ptr = readdir(dir))){
        if(dir_ptr->d_type == DT_CHR || dir_ptr->d_type == DT_BLK){
            struct stat st;
            snprintf(buf,sizeof(buf)-1,"%s/%s",path,dir_ptr->d_name);
            if(stat(buf,&st)==0){
                int rdev = st.st_rdev;
                if(MAJOR(rdev)==major && MINOR(rdev) == minor){
                    ret = strdup(buf);
                    break;
                }
            }else{
                USBVIEW_LOG("stat %s failed!%s",buf,strerror(errno));
            }
        }else if(dir_ptr->d_type == DT_DIR &&
                 memcmp(dir_ptr->d_name,".",1) !=0){
            snprintf(buf,sizeof(buf)-1,"%s/%s",path,dir_ptr->d_name);
            ret = usb_deep_find_path(buf,major,minor);
            if(ret)
                break;
        }
    }
    closedir(dir);
    USBVIEW_LOG("result=%s",ret?ret:"(null)");

    return ret;
}


static char* usb_find_path(u_int8_t major,u_int8_t minor)
{
    char* ret = NULL;
    ret = usb_deep_find_path(SYSFS_DEV_PATH,major,minor);
    return ret;
}

static char *usb_deep_readfile_va(int deep,const char* root,va_list args)
{
    char *ret = NULL,*s = NULL;
    char path[PATH_MAX]={0x00};
    int i;
    FILE* f = NULL;
    DIR* dir = NULL;
    struct dirent *dir_ptr = NULL;
    char p[PATH_MAX]={0x00};

    strncpy(path,root,sizeof(path));
    for(i=0;i+1<deep;i++){
        s = va_arg(args,char*);
        if(strchr(s,'*')||strchr(s,'?')||(strchr(s,'[')&&strchr(s,']'))){
            // use pattern match
            dir = opendir(path);

            if(!dir)
                goto exit;
            while((dir_ptr = readdir(dir))){
                if((dir_ptr->d_type == DT_DIR)&&(fnmatch(s,dir_ptr->d_name,FNM_PATHNAME|FNM_PERIOD) == 0)){
                    snprintf(p,sizeof(p),"%s%s/",path,dir_ptr->d_name);
                    ret =  usb_deep_readfile_va(deep-i-1,p,args);
                    if(ret){
                        closedir(dir);
                        goto exit;
                    }
                }
            }
            closedir(dir);
        }else{
            strcat(path,s);
            strcat(path,"/");
        }
    }

    s = va_arg(args,char*);

    strcat(path,s);

    f = fopen(path,"r");
    if(f){
        long len;

        fseek(f,0,SEEK_END);
        len = ftell(f);
        fseek(f,0,SEEK_SET);

        ret = (char*)malloc(len+1);
        fread(ret,1,len,f);
        ret[len] = 0;

        fclose(f);
    }

exit:
    return ret;
}

static char *usb_deep_readfile(int deep,...)
{
    char* ret;
    va_list args;

    va_start(args, deep);
    ret = usb_deep_readfile_va(deep,"/",args);
    va_end(args);

    return ret;
}

static int usb_get_device_number(usb_device_info* device,usb_device_config* config,usb_device_interface* interface,u_int8_t *major,u_int8_t *minor)
{
    int ret = -1;
    char path[PATH_MAX];
    char tmp[PATH_MAX];
    int len = 0 ,tlen = 0;
    int level = device->level;
    usb_device_info* parent = device->parent;
    DIR *dir = NULL;
    struct dirent *dir_ptr = NULL;

    USBVIEW_LOG("Get Device number of %d:%d-%d.%d",
        device->busnum,device->devnum,config->bConfigurationValue,interface->bInterfaceNumber);

    // if level == 0 use (bus)-0
    tlen += snprintf(tmp+tlen,sizeof(tmp)-tlen-1,"%d",level?device->portNumber+1:0);
    for(;level>1&&parent;level--){
        tlen += snprintf(tmp+tlen,sizeof(tmp)-tlen-1,".%d",parent->portNumber+1);
        parent = parent->parent;
    }

    // if parent not found
    if(level&&!parent){
        USBVIEW_LOG_ERROR("[%d:%d-%d.%d] break off when finding parent!",
                  device->busnum,device->devnum,config->bConfigurationValue,interface->bInterfaceNumber);
        return -1;
    }else{
        strRev(tmp);
        len += snprintf(path,sizeof(path)-len-1,"%s/%d-%s:%d.%d",
                        SYSFS_DEVICE_PATH,device->busnum,
                        tmp,config->bConfigurationValue,interface->bInterfaceNumber);
    }

    // I only known hid and printer
    // update me
    dir = opendir(path);
    if(dir){
        char *buf = NULL;
        while((dir_ptr = readdir(dir))){
            if(dir_ptr->d_type == DT_DIR && memcmp(dir_ptr->d_name,"usb",3)==0 ){
                buf = usb_deep_readfile(4,path,"usb*","*","dev");
                if(buf && sscanf(buf,"%d:%d",major,minor)==2){
                    ret = 0;
                    free(buf);
                    break;
                }
                if(buf)
                    free(buf);
            }else if(dir_ptr->d_type == DT_DIR && strchr(dir_ptr->d_name,':')){
                buf = usb_deep_readfile(5,path,"*:*:*.*","*","*","dev");
                if(buf && sscanf(buf,"%d:%d",major,minor)==2){
                    ret = 0;
                    free(buf);
                    break;
                }
                if(buf)
                    free(buf);
            }
        }

        closedir(dir);
    }

    if(ret){
        USBVIEW_LOG("get device number failed!");
    }else{
        USBVIEW_LOG("get device number succeed: major=%d minor=%d",*major,*minor);
    }

    return ret;
}

/**********************Funs of usb-devices****************************************/

static void AddDevice (usb_device_info** proot,const char *line)
{
    NEW(device,usb_device_info);
    int parentNumber;
    usb_device_info *tmp;

    /* parse the line */
    device->busnum          = GetInt (line, TOPOLOGY_BUS_STRING, 16);
    device->level           = GetInt (line, TOPOLOGY_LEVEL_STRING, 16);
    parentNumber            = GetInt (line, TOPOLOGY_PARENT_STRING, 10);
    device->portNumber      = GetInt (line, TOPOLOGY_PORT_STRING, 16);
    device->connectorNumber = GetInt (line, TOPOLOGY_COUNT_STRING, 16);
    device->devnum          = GetInt (line, TOPOLOGY_DEVICENUMBER_STRING, 10);
    device->speed           = GetInt (line, TOPOLOGY_SPEED_STRING, 10);
    device->maxchild        = GetInt (line, TOPOLOGY_MAXCHILDREN_STRING, 10);

    device->next = NULL;

    // why? what happened?
    if (device->devnum == -1)
        device->devnum = 0;

    /* Set up the parent / child relationship */
    if(device->maxchild){
        device->children = (usb_device_info**)malloc(device->maxchild*sizeof(usb_device_info*));
        memset(device->children,0,device->maxchild*sizeof(usb_device_info*));
    }

    if (device->level == 0) {
        /* this is the root, don't go looking for a parent */
        device->parent = NULL;
    } else {
        /* need to find this device's parent */
        /* parent must be found before children*/
        device->parent = usb_find_device (parentNumber, device->busnum,*proot);
        if (device->parent) {
            if(device->parent->maxchild){
                int i;
                for(i=0;i<device->parent->maxchild&&device->parent->children[i]!=NULL;++i);
                if(i<device->parent->maxchild){
                    device->parent->children[i] = device;
                }else{
                    USBVIEW_LOG_ERROR("parrent(%02x:%02x) has too much child!",device->parent->busnum,device->parent->devnum);
                }
            }else{
                USBVIEW_LOG_ERROR("parent(%02x:%02x) has no child!",device->parent->busnum,device->parent->devnum);
            }
        }else{
            USBVIEW_LOG_ERROR("can't find parent(%02x:%02x)!",device->busnum,parentNumber);
        }
    }

    if(*proot){
        tmp = *proot;
        while(tmp->next){
            tmp = tmp->next;
        }
        tmp->next = device;
    }else{
        // root NULL, this device is root
        *proot = device;
    }
}



static void AddDeviceInformation (usb_device_info* device,const char *data)
{
    if (device == NULL){
        USBVIEW_LOG_ERROR("device null!");
        return;
    }

    if(device->version){
        USBVIEW_LOG_ERROR("device(%02x:%02x) already had version!",device->busnum,device->devnum);
        return;
    }

    device->version             = GetString(data,DEVICE_VERSION_STRING,1);
    device->bDeviceClass        = GetInt (data, DEVICE_CLASS_STRING,16);
    device->bDeviceSubClass     = GetInt (data, DEVICE_SUBCLASS_STRING,16);
    device->bDeviceProtocol     = GetInt (data, DEVICE_PROTOCOL_STRING,16);
    device->bMaxPacketSize0     = GetInt (data, DEVICE_MAXPACKETSIZE_STRING, 10);
    device->bNumConfigurations  = GetInt (data, DEVICE_NUMCONFIGS_STRING, 10);

    if(device->bNumConfigurations){
        device->config = (usb_device_config**)malloc(device->bNumConfigurations*sizeof(usb_device_config*));
        memset(device->config,0,device->bNumConfigurations*sizeof(usb_device_config*));
    }
}



static void AddMoreDeviceInformation (usb_device_info* device,const char *data)
{
    if (device == NULL){
        USBVIEW_LOG_ERROR("device null!");
        return;
    }
    if(device->bcdDevice){
        USBVIEW_LOG_ERROR("device(%02x:%02x) already had bcdDevice!",device->busnum,device->devnum);
        return;
    }

    device->idVendor        = GetInt (data, DEVICE_VENDOR_STRING, 16);
    device->idProduct       = GetInt (data, DEVICE_PRODUCTID_STRING, 16);
    device->bcdDevice       = GetString (data, DEVICE_REVISION_STRING,1);
}


static void AddDeviceString (usb_device_info* device,const char *data)
{
    if (device == NULL){
        USBVIEW_LOG_ERROR("device null!");
        return;
    }

    if (strstr (data, DEVICE_MANUFACTURER_STRING) != NULL) {
        if(device->manufacturer){
            USBVIEW_LOG_ERROR("device(%02x:%02x) already had manufacturer!",
                      device->busnum,device->devnum);
        }else{
            device->manufacturer = GetString(data,DEVICE_MANUFACTURER_STRING,0);
        }
    }else   if (strstr (data, DEVICE_PRODUCT_STRING) != NULL) {
        if(device->product){
            USBVIEW_LOG_ERROR("device(%02x:%02x) already had product!",
                      device->busnum,device->devnum);
        }else{
            device->product = GetString(data,DEVICE_PRODUCT_STRING,0);
        }
    }else if (strstr (data, DEVICE_SERIALNUMBER_STRING) != NULL) {
        if(device->serial){
            USBVIEW_LOG_ERROR("device(%02x:%02x) already had serial number!",
                      device->busnum,device->devnum);
        }else{
            device->serial = GetString(data,DEVICE_SERIALNUMBER_STRING,0);
        }
    }
}


static void AddBandwidth (usb_device_info *device,const char *data)
{
    usb_device_bandwidth *bandwidth;

    if (device == NULL){
        USBVIEW_LOG_ERROR("device null!");
        return;
    }
    if(device->bandwidth){
        USBVIEW_LOG_ERROR("device(%02x:%02x) already had bandwidth!",
                  device->busnum,device->devnum);
        return;
    }

    bandwidth = (usb_device_bandwidth *)malloc (sizeof(usb_device_bandwidth));
    memset(bandwidth,0,sizeof(usb_device_bandwidth));

    bandwidth->allocated            = GetInt (data, BANDWIDTH_ALOCATED, 10);
    // not perfect,update me
    bandwidth->total                = GetInt (data, BANDWIDTH_TOTAL, 10);
    bandwidth->numInterruptRequests = GetInt (data, BANDWIDTH_INTERRUPT_TOTAL, 10);
    bandwidth->numIsocRequests      = GetInt (data, BANDWIDTH_ISOC_TOTAL, 10);

    device->bandwidth = bandwidth;

    return;
}


static void AddConfig (usb_device_info* device,const char *data)
{
    usb_device_config    *config;
    int             i;

    if (device == NULL){
        USBVIEW_LOG_ERROR("device null!");
        return;
    }

    if(device->bNumConfigurations == 0){
        USBVIEW_LOG_ERROR("device(%02x:%02x) has no config",
                  device->busnum,device->devnum);
        return;
    }

    /* Find the next available config in this device */
    for (i = 0; i < device->bNumConfigurations && device->config[i]; ++i);
    if (i >= device->bNumConfigurations) {
        /* ran out of room to hold this config */
        USBVIEW_LOG_ERROR("device(%02x:%02x) has too many configs!",
                   device->busnum,device->devnum);
        return;
    }

    config = (usb_device_config *)malloc (sizeof(usb_device_config));
    memset(config,0,sizeof(usb_device_config));

    config->bNumInterfaces          = GetInt (data, CONFIG_NUMINTERFACES_STRING, 10);
    config->bConfigurationValue     = GetInt (data, CONFIG_CONFIGNUMBER_STRING, 10);
    config->bmAttributes            = GetInt (data, CONFIG_ATTRIBUTES_STRING, 16);
    config->bMaxPower               = GetInt (data,CONFIG_MAXPOWER_STRING,10);

    if(config->bNumInterfaces){
        config->interfaces = (usb_device_interface**)malloc(config->bNumInterfaces*sizeof(usb_device_interface*));
        memset(config->interfaces,0,config->bNumInterfaces*sizeof(usb_device_interface*));
    }

    /* have the device now point to this config */
    device->config[i] = config;
}



static void AddInterface (usb_device_info* device,const char *data)
{
    usb_device_config    *config;
    usb_device_interface *interface;
    int             i;


    if (device == NULL){
        USBVIEW_LOG_ERROR("device null!");
        return;
    }

    if(device->bNumConfigurations == 0){
        USBVIEW_LOG_ERROR("device(%02x:%02x) has no config",
                  device->busnum,device->devnum);
        return;
    }

    /* find the LAST config in the device */
    for (i = 0; i < device->bNumConfigurations && device->config[i]; ++i);

    if(i==0){
        USBVIEW_LOG_ERROR("none of device(%02x:%02x) configs init!",
                  device->busnum,device->devnum);
        return;
    }
    // swich to last config
    i--;

    config = device->config[i];

    /* now find a place in this config to place the interface */
    if(config->bNumInterfaces == 0){
        USBVIEW_LOG_ERROR("config(%02x:%02x-%d) has no interface!",
                  device->busnum,device->devnum,config->bConfigurationValue);
        return;
    }

    for(i=0;i<config->bNumInterfaces && config->interfaces[i];++i);
    if(i>=config->bNumInterfaces){
        // it often happens,shall we show log?
#if 0
        USBVIEW_LOG_ERROR("config(%02x:%02x-%d) has too many interfaces!",
                  device->busnum,device->devnum,config->bConfigurationValue);
#else
        USBVIEW_LOG("config(%02x:%02x:%d) has too many interfaces!",
                  device->busnum,device->devnum,config->bConfigurationValue);
#endif
        return;
    }

    interface = (usb_device_interface *)malloc (sizeof(usb_device_interface));
    memset(interface,0,sizeof(usb_device_interface));

    interface->bInterfaceNumber     = GetInt (data, INTERFACE_NUMBER_STRING, 10);
    interface->bAlternateSetting    = GetInt (data, INTERFACE_ALTERNATESETTING_STRING, 10);
    interface->bNumEndpoints        = GetInt (data, INTERFACE_NUMENDPOINTS_STRING, 10);
    interface->bInterfaceClass      = GetInt (data, INTERFACE_CLASS_STRING, 16);
    interface->bInterfaceSubClass   = GetInt (data, INTERFACE_SUBCLASS_STRING, 16);
    interface->bInterfaceProtocol   = GetInt (data, INTERFACE_PROTOCOL_STRING, 16);
    interface->driver               = GetString(data,INTERFACE_DRIVERNAME_STRING,1);
    if(interface->driver&&strcmp(interface->driver,INTERFACE_DRIVERNAME_NODRIVER_STRING)!=0){
        interface->attached = 1;
    }else{
        interface->attached = 0;
    }

    if(interface->bNumEndpoints){
        interface->endpoint = (usb_device_endpoint**)malloc(interface->bNumEndpoints*sizeof(usb_device_endpoint*));
        memset(interface->endpoint,0,interface->bNumEndpoints*sizeof(usb_device_endpoint*));
    }

    // find path
    if(interface->attached){
        u_int8_t major = 0,minor = 0;
        if(usb_get_device_number(device,config,interface,&major,&minor)==0){
            interface->path = usb_find_path(major,minor);
        }
    }

    /* now point the config to this interface */
    config->interfaces[i] = interface;
}



static void AddEndpoint (usb_device_info* device,const char *data)
{
    usb_device_config    *config;
    usb_device_interface *interface;
    usb_device_endpoint  *endpoint;
    int             i;

    if (device == NULL){
        USBVIEW_LOG_ERROR("device null!");
        return;
    }

    /* find the LAST config in the device */
    if(device->bNumConfigurations == 0){
        USBVIEW_LOG_ERROR("device(%02x:%02x) has no config",
                  device->busnum,device->devnum);
        return;
    }
    for (i = 0; i < device->bNumConfigurations && device->config[i]; ++i);

    if(i==0){
        USBVIEW_LOG_ERROR("none of device(%02x:%02x) configs init!",
                  device->busnum,device->devnum);
        return;
    }
    // swich to last config
    i--;
    config = device->config[i];

    /* find the LAST interface in the config */
    if(config->bNumInterfaces == 0){
        USBVIEW_LOG_ERROR("config(%02x:%02x-%d) has no interface!",
                  device->busnum,device->devnum,config->bConfigurationValue);
        return;
    }

    for(i=0;i<config->bNumInterfaces && config->interfaces[i];++i);
    if(i==0){
        USBVIEW_LOG_ERROR("none of config(%02x:%02x-%d) interfaces init!",
                  device->busnum,device->devnum,config->bConfigurationValue);
        return;
    }
    // swich to last interface
    i--;
    interface = config->interfaces[i];

    /* now find a place in this interface to place the endpoint */
    if(interface->bNumEndpoints == 0){
        USBVIEW_LOG_ERROR("interface(%02x:%02x-%d.%d) has no endpoint!",
                  device->busnum,device->devnum,config->bConfigurationValue,interface->bInterfaceNumber);
        return;
    }
    for(i=0;i<interface->bNumEndpoints && interface->endpoint[i];++i);
    if(i>=interface->bNumEndpoints){
        // it often happens,shall we show log?
#if 0
        USBVIEW_LOG_ERROR("interface(%02x:%02x-%d.%d) too many endpoints!",
                  device->busnum,device->devnum,config->bConfigurationValue,interface->bInterfaceNumber);
#else
        USBVIEW_LOG("interface(%02x:%02x:%d:%d) too many endpoints!",
                  device->busnum,device->devnum,config->bConfigurationValue,interface->bInterfaceNumber);
#endif
        return;
    }

    endpoint = (usb_device_endpoint *)malloc (sizeof(usb_device_endpoint));
    memset(endpoint,0,sizeof(usb_device_endpoint));

    endpoint->bEndpointAddress      = GetInt (data, ENDPOINT_ADDRESS_STRING, 16);
    endpoint->bmAttributes          = GetInt (data, ENDPOINT_ATTRIBUTES_STRING, 16);
    endpoint->wMaxPacketSize        = GetInt (data, ENDPOINT_MAXPACKETSIZE_STRING, 10);
    endpoint->bInterval             = GetInt (data, ENDPOINT_INTERVAL_STRING, 10);

    // not perfect
    if(strstr(data,"(I)")){
        endpoint->in = USB_ENDPOINT_IN;
    }else if(strstr(data,"(O)")){
        endpoint->in = USB_ENDPOINT_OUT;
    }else{
        USBVIEW_LOG_ERROR("we don't know which type of endpoint it is![%s]",data);
    }

    /* point the interface to the endpoint */
    interface->endpoint[i] = endpoint;
}

static const char sysfs_usb_devices_files[][40]={
    "/proc/bus/usb/devices",
    "/sys/kernel/debug/usb/devices"
};

#define USB_DEVICES_FILES_NUM  (sizeof(sysfs_usb_devices_files)/sizeof(sysfs_usb_devices_files[0]))
static int sysfs_has_usb_devices = -1;

// if is_sysfs_has_usb_devices==0,shall we use usb-devices(linux version>=2.6.31)?
static int is_sysfs_has_usb_devices()
{
    if(-1 == sysfs_has_usb_devices){
        struct stat statbuf;
        int r,i;

        sysfs_has_usb_devices = 0;
        for(i=0;i<USB_DEVICES_FILES_NUM;++i){
            r = stat(sysfs_usb_devices_files[i],&statbuf);
            if( (r==0) && (S_ISREG(statbuf.st_mode))){
                sysfs_has_usb_devices = i+1;
                break;
            }else{
                USBVIEW_LOG("stat %s error!%s",sysfs_usb_devices_files[i],strerror(errno));
            }
        }
    }

    return sysfs_has_usb_devices;
}

static void usb_parse_line (char * line,usb_device_info** proot)
{
    usb_device_info* lastDevice = NULL;
    usb_device_info* tmp = NULL;

    if(*proot){
        tmp = *proot;
        while(tmp->next){
            tmp = tmp->next;
        }
        lastDevice = tmp;
    }

    /* chop off the trailing \n */
    if(line[strlen(line)-1]='\n')
        line[strlen(line)-1] = 0x00;

    /* look at the first character to see what kind of line this is */
    switch (line[0]) {
        case 'T': /* topology */
            AddDevice (proot,line);
            break;

        case 'B': /* bandwidth */
            AddBandwidth (lastDevice, line);
            break;

        case 'D': /* device information */
            AddDeviceInformation (lastDevice, line);
            break;

        case 'P': /* more device information */
            AddMoreDeviceInformation (lastDevice, line);
            break;

        case 'S': /* device string information */
            AddDeviceString (lastDevice, line);
            break;

        case 'C': /* config descriptor info */
            AddConfig (lastDevice, line);
            break;

        case 'I': /* interface descriptor info */
            AddInterface (lastDevice, line);
            break;

        case 'E': /* endpoint descriptor info */
            AddEndpoint (lastDevice, line);
            break;

        default:
            break;
    }

    return;
}

# if 0 // android gcc has not getline
static usb_device_info* sysfs_read_usb_devices(const char* file)
{
    FILE* f;
    usb_device_info* device_info = NULL;

    f = fopen(file,"r");
    if(!f){
        USBVIEW_LOG_ERROR("open %s failed!%s",file,strerror(errno));
        return NULL;
    }

    for(;;){
        ssize_t readed = -1;
        char* line = NULL;
        size_t len = 0;

        if(feof(f))
            break;

        /* read one line */
        readed = getline(&line,&len,f);
        if(readed<=0||!line||!len)
            break;
        /* chop off the trailing \n */
        line[strlen(line)-1] = 0x00;
        USBVIEW_LOG("line=%s",line);
        usb_parse_line(line,&device_info);

        free(line);
    }

    fclose(f);
    return device_info;
}
#else
#define READBUFSIZE     (2*1024)
static usb_device_info* sysfs_read_usb_devices(const char* file)
{
    FILE* f;
    usb_device_info* device_info = NULL;
    char* line = NULL;

    f = fopen(file,"r");
    if(!f){
        USBVIEW_LOG_ERROR("open %s failed!%s",file,strerror(errno));
        return NULL;
    }

    line = malloc(READBUFSIZE);
    if (line == NULL) {
        fclose(f);
        return NULL;
    }

    for(;;){

        if(feof(f))
            break;

        memset(line, 0 , READBUFSIZE);
        if (fgets(line, READBUFSIZE - 1, f) == NULL)
            break;

        USBVIEW_LOG("line=%s",line);
        usb_parse_line(line,&device_info);
    }

    free(line);

    fclose(f);
    return device_info;
}
#endif


extern usb_device_info *get_usb_devices()
{
    usb_device_info* device_info = NULL;

    if(is_sysfs_has_usb_devices()){
        device_info = sysfs_read_usb_devices(sysfs_usb_devices_files[sysfs_has_usb_devices-1]);
        if(device_info)
            goto exit;
    }
exit:
    return device_info;
}

extern void free_usb_devices (usb_device_info* device)
{
    usb_device_info *tmp;
    while(device){
        int i;
        tmp = device->next;

        free (device->name);
        free (device->version);
        free (device->bcdDevice);
        free (device->manufacturer);
        free (device->product);
        free (device->serial);
        for(i=0;i<device->bNumConfigurations;i++){
            DestroyConfig(device->config[i]);
        }
        free(device->config);
        free(device->children);
        free(device->bandwidth);

        free(device);

        device = tmp;
    }
}

// not complete,update me
static const char usb_class_code_string[][20]={
    ">ifc",
    "audio",
    "vcom",
    "HID",
    "",
    "physical",
    "image",
    "print",
    "store",
    "hub",
    "data",
    "smart card",
    "",
    "security",
    "video",
    "per.health"
};

extern const char* parse_usb_class_code(int class_code)
{
    switch(class_code){
    case USB_CLASS_PER_INTERFACE:
    case USB_CLASS_AUDIO:
    case USB_CLASS_COMM:
    case USB_CLASS_HID:
    case USB_CLASS_PHYSICAL:
    case USB_CLASS_PRINTER:
    case USB_CLASS_IMAGE:
    case USB_CLASS_MASS_STORAGE:
    case USB_CLASS_HUB:
    case USB_CLASS_DATA:
    case USB_CLASS_SMART_CARD:
    case USB_CLASS_CONTENT_SECURITY:
    case USB_CLASS_VIDEO:
    case USB_CLASS_PERSONAL_HEALTHCARE:
        return usb_class_code_string[class_code];
    case USB_CLASS_DIAGNOSTIC_DEVICE:
        return "diagnostic";
    case USB_CLASS_WIRELESS:
        return "wlen";
    case USB_CLASS_APPLICATION:
        return "app.";
    case USB_CLASS_VENDOR_SPEC:
        return "vendor";
    default:
        return "unkown";
    }
}

// not complete,update me
static const char usb_transfer_type_string[][10]={
    "Cont.",
    "ISO.",
    "Bulk",
    "Int.",
    "Stream"
};
extern const char* parse_usb_transfer_type(int transfer_type)
{
    switch(transfer_type){
    case USB_TRANSFER_TYPE_CONTROL:
    case USB_TRANSFER_TYPE_ISOCHRONOUS:
    case USB_TRANSFER_TYPE_BULK:
    case USB_TRANSFER_TYPE_INTERRUPT:
    case USB_TRANSFER_TYPE_BULK_STREAM:
        return usb_transfer_type_string[transfer_type];
    default:
        return "unkown";
    }
}
