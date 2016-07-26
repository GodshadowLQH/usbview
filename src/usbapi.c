#include "usbapi.h"

#if defined OS_LINUX
#include "linux_netlink.h"
#endif

#define TAG "usbapi"



#ifdef OS_WIN
#pragma error("Update me to support hotplug")
#endif


typedef struct {
    os_mutex_t mutex;
    int max;
    int num;
    usbapi_device **devices;
}usbapi_context_t;

static usbapi_context_t context =
{
    .max=0,
    .num=-1,
    .devices=NULL
};

/* Linked List of input reports received from the device. */
struct input_report {
    char* data;
    size_t len;
    struct input_report *next;
};

struct usbapi_device_endpoint
{
    /** The address of the endpoint described by this descriptor. Bits 0:3 are
     * the endpoint number. Bits 4:6 are reserved. Bit 7 indicates direction,
     * see \ref libusb_endpoint_direction.
     */
    uint8_t  addr;

    /** Attributes which apply to the endpoint when it is configured using
     * the bConfigurationValue. Bits 0:1 determine the transfer type and
     * correspond to \ref libusb_transfer_type. Bits 2:3 are only used for
     * isochronous endpoints and correspond to \ref libusb_iso_sync_type.
     * Bits 4:5 are also only used for isochronous endpoints and correspond to
     * \ref libusb_iso_usage_type. Bits 6:7 are reserved.
     */
    uint8_t  attr;

    /** Maximum packet size this endpoint is capable of sending/receiving. */
    uint16_t max;

    /** Interval for polling endpoint for data transfers. */
    uint8_t  ivl;
};

struct usbapi_device{
    /* Handle to the actual device. */
    HANDLE handle;
    usbapi_device_info *info;

    /* Read thread objects */
    os_thread_t thread;
    os_mutex_t buffer_mutex; /* Protects input_reports */
    os_cond_t condition;
#ifdef OS_LINUX
    int thread_pipe[2];
#endif

    os_mutex_t dev_mutex;
    int shutdown_thread;

    /* List of received input reports. */
    struct input_report *input_reports;
#define DEFAULT_MAX_INPUT_REPORTS 100
};

static usbapi_device *new_usbapi_device(void)
{
    usbapi_device *dev = (usbapi_device*)malloc(sizeof(usbapi_device));

    if(!dev){
        LOGE(TAG,"calloc failed!");
        return NULL;
    }
    dev->handle=INVALID_HANDLE_VALUE;
    dev->info=NULL;
    dev->input_reports=NULL;

    dev->shutdown_thread=0;
    os_mutex_init(dev->buffer_mutex);
    os_cond_init(dev->condition);
    os_mutex_init(dev->dev_mutex);
#ifdef OS_LINUX
    dev->thread_pipe[0] = -1;
    dev->thread_pipe[1] = -1;
#endif

    return dev;
}

static void free_usbapi_device(usbapi_device *dev)
{
    usbapi_flush(dev);
    /* Clean up the info objects */
    usbapi_free_enumeration(dev->info);
    dev->info = NULL;
    /* Clean up the thread objects */
    os_cond_destroy(dev->condition);
    os_mutex_destroy(dev->buffer_mutex);
    os_mutex_destroy(dev->dev_mutex);

    /* Free the device itself */
    free(dev);

    LOGD(TAG,"free devices %p success.",dev);
}


usbapi_device_info *usbapi_enumerate(unsigned short vendor_id, unsigned short product_id)
{
    usb_device_info* root_info = NULL,* info = NULL;
    usbapi_device_info *root = NULL; /* return object */
    usbapi_device_info *cur_dev = NULL;
    int i,j,k;

    root_info = info = get_usb_devices();
    while(info) {
        if((vendor_id==0&&product_id==0)||
                (vendor_id == info->idVendor && product_id == info->idProduct)){
            for(i=0;i<info->bNumConfigurations&&info->config[i];i++){ // generally, usb device has only one config
                usb_device_config* config = info->config[i];
                for(j=0;j<config->bNumInterfaces&&config->interfaces[j];j++){
                    usb_device_interface* inf = config->interfaces[j];
                    // got right device
                    usbapi_device_info *tmp;
                    /* VID/PID match. Create the record. */
                    tmp = calloc(1, sizeof(struct usbapi_device_info));
                    if(!tmp){
                        LOGE(TAG,"calloc failed!");
                        goto next;
                    }

                    memset((void*)tmp,0,sizeof(struct usbapi_device_info));
                    if (cur_dev) {
                        cur_dev->next = tmp;
                    }
                    else {
                        root = tmp;
                    }
                    cur_dev = tmp;

                    /* Fill out the record */
                    cur_dev->path = inf->path?strdup(inf->path):NULL;
                    cur_dev->product_id = info->idProduct;
                    cur_dev->vendor_id = info->idVendor;
                    cur_dev->serial_number = info->serial?strdup(info->serial):NULL;
                    cur_dev->release_number = info->bcdDevice?(unsigned short)(atof(info->bcdDevice)*100):0;
                    cur_dev->manufacturer_string = info->manufacturer?strdup(info->manufacturer):NULL;
                    cur_dev->product_string = info->product?strdup(info->product):NULL;
                    cur_dev->usage = 0;
                    cur_dev->interface_number = inf->bInterfaceNumber;
                    cur_dev->class_code = inf->bInterfaceClass;
                    cur_dev->busnum = info->busnum;
                    cur_dev->devnum = info->devnum;
                    cur_dev->input_endpoint = NULL;
                    cur_dev->output_endpoint = NULL;
                    for(k=0;k<inf->bNumEndpoints&&inf->endpoint[k];k++){
                        usb_device_endpoint* ep = inf->endpoint[k];
                        if(ep->in == USB_ENDPOINT_IN && !cur_dev->input_endpoint){
                            cur_dev->input_endpoint = (struct usbapi_device_endpoint *)malloc(sizeof(struct usbapi_device_endpoint));
                            cur_dev->input_endpoint->addr = ep->bEndpointAddress;
                            cur_dev->input_endpoint->attr = ep->bmAttributes;
                            cur_dev->input_endpoint->ivl = ep->bInterval;
                            cur_dev->input_endpoint->max = ep->wMaxPacketSize;
                        }else if(ep->in== USB_ENDPOINT_OUT && !cur_dev->output_endpoint){
                            cur_dev->output_endpoint = (struct usbapi_device_endpoint *)malloc(sizeof(struct usbapi_device_endpoint));
                            cur_dev->output_endpoint = (struct usbapi_device_endpoint *)malloc(sizeof(struct usbapi_device_endpoint));
                            cur_dev->output_endpoint->addr = ep->bEndpointAddress;
                            cur_dev->output_endpoint->attr = ep->bmAttributes;
                            cur_dev->output_endpoint->ivl = ep->bInterval;
                            cur_dev->output_endpoint->max = ep->wMaxPacketSize;
                        }
                    }

                    cur_dev->next = NULL;
                }
            }
        }
next:
        info = info->next;
    }

    free_usb_devices(root_info);

    return root;
}

usbapi_device_info*  dup_usbapi_info(usbapi_device_info *dev_info)
{
    usbapi_device_info* ret;

    if(!dev_info)
        return NULL;

    ret = calloc(1,sizeof(usbapi_device_info));

    memcpy(ret,dev_info,sizeof(usbapi_device_info));
    ret->path = dev_info->path?strdup(dev_info->path):NULL;
    ret->serial_number = dev_info->serial_number?strdup(dev_info->serial_number):NULL;
    ret->manufacturer_string = dev_info->manufacturer_string?strdup(dev_info->manufacturer_string):NULL;
    ret->product_string = dev_info->product_string?strdup(dev_info->product_string):NULL;
    if(dev_info->input_endpoint){
        ret->input_endpoint = (struct usbapi_device_endpoint*)malloc(sizeof(struct usbapi_device_endpoint));
        memcpy(ret->input_endpoint,dev_info->input_endpoint,sizeof(struct usbapi_device_endpoint));
    }
    if(dev_info->output_endpoint){
        ret->output_endpoint = (struct usbapi_device_endpoint*)malloc(sizeof(struct usbapi_device_endpoint));
        memcpy(ret->output_endpoint,dev_info->output_endpoint,sizeof(struct usbapi_device_endpoint));
    }
    ret->next = NULL;

    return ret;
}

void  usbapi_free_enumeration(usbapi_device_info *devs)
{
    usbapi_device_info *d = devs;
    while (d) {
        usbapi_device_info *next = d->next;
        free(d->path);
        free(d->serial_number);
        free(d->manufacturer_string);
        free(d->product_string);
        free(d->input_endpoint);
        free(d->output_endpoint);
        free(d);
        d = next;
    }
}

/* Helper function, to simplify hid_read().
   This should be called with dev->buffer_mutex locked. */
static int return_data(usbapi_device *dev, char *data, size_t length)
{
    /* Copy the data out of the linked list item (rpt) into the
       return buffer (data), and delete the liked list item. */
    struct input_report *rpt = dev->input_reports;
    size_t len;

    len = (length < rpt->len)? length: rpt->len;
    if (len > 0)
        memcpy(data, rpt->data, len);
    dev->input_reports = rpt->next;

    free(rpt->data);
    free(rpt);
    return len;
}


#if defined OS_LINUX
static void *read_thread(void *param)
#elif defined OS_WIN
static DWORD WINAPI *read_thread(LVOID param)
#endif
{
    usbapi_device *dev = param;
    unsigned char buf[2048];
    int bytes_read;
    int res;

    if(!dev||!dev->info){
        LOGE(TAG,"Invalid parameter!");
        return NULL;
    }

    if(!dev->info->input_endpoint){
        LOGE(TAG,"Input endpoint not exist!");
        /* Notify the main thread that the read thread is up and running. */
        return NULL;
    }

    LOGD(TAG,"read thread start!");

    /* Handle all the events. */
    while (1) {
        if(dev->shutdown_thread || dev->handle == INVALID_HANDLE_VALUE){
            break;
        }
#ifdef OS_LINUX
        struct pollfd fds[] = {
            { .fd = dev->thread_pipe[0],
              .events = POLLIN },
            { .fd = dev->handle,
              .events = POLLIN },
        };
        res = poll(fds,2,-1);
        if( res>0 && (fds[1].revents & POLLIN) ){
            // has data
            res = 1;
        }else{
            res = 0;
        }
#else
        os_select(dev->handle,INVALID_HANDLE_VALUE,1000,res);
#endif
        if(res>0){
            // has data
            memset(buf,0,sizeof(buf));
            bytes_read = -1;
            os_read(dev->handle,buf,sizeof(buf),bytes_read);
            if(bytes_read>0){
                struct input_report *rpt = (struct input_report*)malloc(sizeof(struct input_report));
                rpt->data = malloc(bytes_read);
                memcpy(rpt->data, buf, bytes_read);
                rpt->len = bytes_read;
                rpt->next = NULL;

                os_mutex_lock(dev->buffer_mutex);
                /* Attach the new report object to the end of the list. */
                if (dev->input_reports == NULL) {
                    /* The list is empty. Put it at the root. */
                    dev->input_reports = rpt;
                    os_cond_signal(dev->condition);
                } else {
                    /* Find the end of the list and attach. */
                    struct input_report *cur = dev->input_reports;
                    int num_queued = 1;
                    while (cur->next != NULL) {
                        cur = cur->next;
                        num_queued++;
                    }
                    cur->next = rpt;

                    /* Pop one off if we've reached DEFAULT_MAX_INPUT_REPORTS in the queue. This
                       way we don't grow forever if the user never reads
                       anything from the device. */
                    if((num_queued >= DEFAULT_MAX_INPUT_REPORTS)){
                        return_data(dev, NULL, 0);
                    }
                }
                os_mutex_unlock(dev->buffer_mutex);
            }
        }else if(res<0){
            // error
        }
    }

    /* Now that the read thread is stopping, Wake any threads which are
       waiting on data (in hid_read_timeout()). Do this under a mutex to
       make sure that a thread which is about to go to sleep waiting on
       the condition acutally will go to sleep before the condition is
       signaled. */
    os_mutex_lock(dev->buffer_mutex);
    os_cond_broadcast(dev->condition);
    os_mutex_unlock(dev->buffer_mutex);

    /* The dev->transfer->buffer and dev->transfer objects are cleaned up
       in hid_close(). They are not cleaned up here because this thread
       could end either due to a disconnect or due to a user
       call to hid_close(). In both cases the objects can be safely
       cleaned up after the call to pthread_join() (in hid_close()), but
       since hid_close() calls libusb_cancel_transfer(), on these objects,
       they can not be cleaned up here. */

    LOGD(TAG,"read thread stop!");
    return NULL;
}

static void usbapi_force_close(usbapi_device *dev);

#if defined OS_LINUX
void usb_plugout(int bus,int dev,const char* sys_name)
{
    int i;
    usbapi_device *device = NULL;
    (void)sys_name;

    LOGD(TAG,"Get plugout:bus=%d dev=%d sys_name=%s",bus,dev,sys_name);
    os_mutex_lock(context.mutex);
    for(i=0;i<context.max&&context.devices[i]!=NULL&&context.devices[i]->info!=NULL;i++){
        usbapi_device_info *info = context.devices[i]->info;
        if(info->busnum == bus && info->devnum == dev){
            device = context.devices[i];
            os_mutex_unlock(context.mutex);
            usbapi_force_close(device);
            os_mutex_lock(context.mutex);
        }
    }
    os_mutex_unlock(context.mutex);
}

#endif

static void register_usbDevice(usbapi_device* dev)
{
    int i;

    if(context.num<0){
        // not init
        os_mutex_init(context.mutex);
        context.num = 0;
    }

    if(!dev)
        return;

    os_mutex_lock(context.mutex);

    if(context.num == 0){
        context.max = 128;
        context.devices = (usbapi_device**)malloc(sizeof(usbapi_device*)*context.max);
        memset(context.devices,0,sizeof(usbapi_device*)*context.max);
#if defined OS_LINUX
        linux_netlink_start_event_monitor(NULL,usb_plugout);
#endif
    }

    for(i=0;i<context.max&&context.devices[i]!=NULL;i++);
    if(i>=context.max){
        // mem not enough
        context.max = context.max*2;
        usbapi_device** devices = (usbapi_device**)malloc(sizeof(usbapi_device*)*context.max);
        memset(devices,0,sizeof(usbapi_device*)*context.max);
        memcpy(devices,context.devices,context.num*sizeof(usbapi_device*));
        free(context.devices);
        context.devices = devices;
    }

    LOGD(TAG,"register device %p with path=%s bus=%02x dev=%02x",
               dev,
               dev->info?dev->info->path:"NULL",
               dev->info?dev->info->busnum:-1,
               dev->info?dev->info->devnum:-1);

    context.devices[i] = dev;
    context.num++;

    os_mutex_unlock(context.mutex);
}

static void deregister_usbDevice(usbapi_device* dev)
{
    int i;

    if(!dev)
        return;

    if(context.num<=0)
        return;

    os_mutex_lock(context.mutex);

    for(i=0;i<context.max ;i++){
    	if(context.devices[i]==dev){
    		// device found
                    LOGD(TAG,"deregister device %p with path=%s bus=%02x dev=%02x",
    		                   dev,
    		                   dev->info?dev->info->path:"NULL",
    		                   dev->info?dev->info->busnum:-1,
    		                   dev->info?dev->info->devnum:-1);
    		        context.devices[i] = NULL;
    		        context.num--;
    		        break;
    	}
    }

    if(context.num==0){
#if defined OS_LINUX
        linux_netlink_stop_event_monitor();
#endif
        context.max = 0;
        free(context.devices);
        context.devices = NULL;
    }
    os_mutex_unlock(context.mutex);

}

#ifdef OS_LINUX
static int create_pipe(int pipefd[2])
{
    int ret = pipe(pipefd);
    if (ret != 0) {
        return ret;
    }
    ret = fcntl(pipefd[1], F_GETFL);
    if (ret == -1) {
        LOGD(TAG,"Failed to get pipe fd flags: %d", errno);
        goto err_close_pipe;
    }
    ret = fcntl(pipefd[1], F_SETFL, ret | O_NONBLOCK);
    if (ret != 0) {
        LOGD(TAG,"Failed to set non-blocking on new pipe: %d", errno);
        goto err_close_pipe;
    }

    return 0;

err_close_pipe:
    close(pipefd[0]);
    close(pipefd[1]);
    return ret;
}

#endif

usbapi_device *  usbapi_open(usbapi_device_info *dev_info)
{
    if(!dev_info)
        return NULL;
    if(!dev_info->path)
        return NULL;

    usbapi_device *dev = new_usbapi_device();
    if(!dev)
        return NULL;

    dev->info = dup_usbapi_info(dev_info);

    /* OPEN HERE */
    dev->handle = os_open(dev->info->path);
    if (dev->handle == INVALID_HANDLE_VALUE) {
#ifdef OS_LINUX
        LOGD(TAG,"can't open device with path=%s : %s",dev->info->path,strerror(errno));
#else
        LOGD(TAG,"can't open device with path=%s",dev->info->path);
#endif

        goto err;
    }

#ifdef OS_LINUX
    if(create_pipe(dev->thread_pipe)!=0){
        os_close(dev->handle);
        LOGE(TAG,"create pipe failed!");
        goto err;
    }
#endif

    LOGD(TAG,"Open usb succeed with path=%s handle=%d",dev->info->path,dev->handle);
    register_usbDevice(dev);
    os_thread_create(dev->thread, read_thread, dev);

    return dev;
err:
    free_usbapi_device(dev);
    return NULL;
}

usbapi_device *  usbapi_open_vid_pid(unsigned short vendor_id, unsigned short product_id)
{
    usbapi_device_info* info,*root_info;
    usbapi_device* dev = NULL;

    info = root_info = usbapi_enumerate(vendor_id,product_id);
    while(info){
        if(info->vendor_id == vendor_id &&
                info->product_id == product_id){
            dev = usbapi_open(info);
            break;
        }
        info = info->next;
    }
    usbapi_free_enumeration(root_info);
    return dev;
}

usbapi_device *  usbapi_open_vid_pid_class(unsigned short vendor_id, unsigned short product_id,enum usb_class_code class_code)
{
    usbapi_device_info* info,*root_info;
    usbapi_device* dev = NULL;

    info = root_info = usbapi_enumerate(vendor_id,product_id);
    while(info){
        if(info->vendor_id == vendor_id &&
                info->product_id == product_id &&
                info->class_code == class_code ){
            dev = usbapi_open(info);
            break;
        }
        info = info->next;
    }
    usbapi_free_enumeration(root_info);
    return dev;
}

int usbapi_isOpen(usbapi_device* dev)
{
    if(dev){
        int ret = 0;
        os_mutex_lock(dev->dev_mutex);
        if(dev->shutdown_thread||dev->handle==INVALID_HANDLE_VALUE){
            ret = 0;
        }else{
            ret = 1;
        }
        os_mutex_unlock(dev->dev_mutex);

        return ret;
    }else{
        return 0;
    }
}

static void usbapi_force_close(usbapi_device *dev)
{
    // close when error or plugout
    if (!dev)
        return;
    // already close
    os_mutex_lock(dev->dev_mutex);
    if(dev->shutdown_thread){
        os_mutex_unlock(dev->dev_mutex);
        return;
    }
    /* Cause read_thread() to stop. */
    dev->shutdown_thread = 1;

    LOGD(TAG,"close %d with path=%s",dev->handle,dev->info?dev->info->path:"NULL");
    deregister_usbDevice(dev);

#ifdef OS_LINUX
    /* Write some dummy data to the control pipe and
     * wait for the thread to exit */
    char dummy = 1;
    int r = write(dev->thread_pipe[1], &dummy, sizeof(dummy));
    if (r <= 0) {
        LOGE(TAG,"control pipe signal failed!");
    }
#endif
    /* Wait for read_thread() to end. */
    LOGD(TAG,"wait for thread exit...");
    os_thread_join(dev->thread);

    os_close(dev->handle);
#ifdef OS_LINUX
    close(dev->thread_pipe[0]);
    close(dev->thread_pipe[1]);
#endif
    // not free device ,user should call usbapi_close
    LOGD(TAG,"close device %d sucess,wait for free %p",dev->handle,dev);
    os_mutex_unlock(dev->dev_mutex);
}

void  usbapi_close(usbapi_device *dev)
{
    if(!dev)
        return;
    usbapi_force_close(dev);
    free_usbapi_device(dev);
}


int usbapi_write_timeout(usbapi_device* dev,const char* data,size_t length,int msecs)
{
    int ret = -1;

    if(!dev){
        LOGD(TAG,"Invalid parameter!");
        return -1;
    }

    if(!data||!length){
        LOGD(TAG,"No data to write!");
        return 0;
    }

    if(dev->handle==INVALID_HANDLE_VALUE){
        LOGD(TAG,"Invalid handle!");
        return -1;
    }

    if(!dev->info->output_endpoint){
        LOGE(TAG,"Output endpoint not exist!");
        return -1;
    }

    LOGD(TAG,"Write %d bytes with timeout=%dms :",length,msecs);
    LOGD_HEX(TAG,data,MIN(40,length));
#if 0
    ret = usbapi_pollout(dev,msecs);
    if(ret>0){
#else
    if(1){
#endif
again:
        os_write(dev->handle,data,length,ret);
        LOGD(TAG,"#### %d bytes written.",ret);
#ifdef OS_LINUX
        if(ret<0){
            LOGD(TAG,"write failed!:%s",strerror(errno));
        	if(errno==EINTR ||
        			errno==EAGAIN){
                LOGD(TAG,"ERROR%d occured,retry again!",errno);
        		goto again;
        	}
        }
#endif

        return ret;
    }else{
#if defined OS_LINUX
        LOGD(TAG,"can not write!%s",ret==0?"No Buf":strerror(errno));
#elif defined OS_WIN
        LOGD(TAG,"can not write!");
#endif
        return ret;
    }
}

int usbapi_write(usbapi_device* dev,const char* data,size_t length)
{
    return usbapi_write_timeout(dev,data,length,0);
}


int usbapi_read_timeout(usbapi_device *dev, char *data, size_t max, int msecs)
{
    int res;

    if(!dev){
        LOGD(TAG,"Invalid parameter!");
        return -1;
    }

    if(!data||!max){
        LOGD(TAG,"No buffer for reading!");
        return 0;
    }

    if(dev->handle==INVALID_HANDLE_VALUE){
        LOGD(TAG,"Invalid handle!");
        return -1;
    }

    LOGD(TAG,"Read %d bytes with timeout=%dms.",max,msecs);

    res = usbapi_pollin(dev,msecs);
    if(res > 0){
        int bytes_read = 0;
        os_mutex_lock(dev->buffer_mutex);
#if 1
        while(dev->input_reports && bytes_read<max){
        	int readed = return_data(dev,data+bytes_read,max-bytes_read);
        	if(readed<=0){
        		break;
        	}else{
        		bytes_read += readed;
        	}
        }
#else
        if (dev->input_reports) {
            /* Return the first one */
            bytes_read = return_data(dev, data, max);
        }
#endif
        os_mutex_unlock(dev->buffer_mutex);
        LOGD(TAG,"#### %d bytes read.",bytes_read);
        if(bytes_read>0)
            LOGD_HEX(TAG,data,MIN(40,bytes_read));

        return bytes_read;
    }else{
#if defined OS_LINUX
        LOGD(TAG,"can not read!%s",res==0?"No data":strerror(errno));
#elif defined OS_WIN
        LOGD(TAG,"can not read!");
#endif
        return res;
    }
}

int usbapi_read(usbapi_device *dev, char *data, size_t max)
{
    return usbapi_read_timeout(dev, data, max, 0);
}

void usbapi_flush(usbapi_device *dev)
{
    if(!dev)
        return;
    os_mutex_lock(dev->buffer_mutex);
    while (dev->input_reports) {
        return_data(dev, NULL, 0);
    }
    os_mutex_unlock(dev->buffer_mutex);
}

int  usbapi_pollin(usbapi_device *dev,int msecs)
{
    int ret = -1;

    if(!dev){
        LOGD(TAG,"Invalid parameter!");
        return -1;
    }

    if(dev->handle==INVALID_HANDLE_VALUE){
        LOGD(TAG,"Invalid handle!");
        return -1;
    }

    os_mutex_lock(dev->buffer_mutex);
    /* There's an input report queued up. Return it. */
    if (dev->input_reports) {
        ret = dev->input_reports->len;
        goto exit;
    }
    if (dev->shutdown_thread) {
        /* This means the device has been disconnected.
           An error code of -1 should be returned. */
        ret = -1;
        goto exit;
    }
    if (msecs == -1) {
        /* Blocking */
        while (!dev->input_reports && !dev->shutdown_thread) {
            os_cond_wait(dev->condition, dev->buffer_mutex);
        }
        if (dev->input_reports)
            ret = dev->input_reports->len;
    }else if (msecs > 0){
        /* Non-blocking, but called with timeout. */
        int res;

        while (!dev->input_reports && !dev->shutdown_thread) {
            os_cond_timedwait(dev->condition, dev->buffer_mutex, msecs,res);
            if (res == 0) {
                if (dev->input_reports) {
                    ret = dev->input_reports->len;
                    break;
                }

                /* If we're here, there was a spurious wake up
                   or the read thread was shutdown. Run the
                   loop again (ie: don't break). */
            }else if (res == ETIMEDOUT) {
                /* Timed out. */
                ret = 0;
                break;
            }else {
                /* Error. */
                ret = -1;
                break;
            }
        }
    }else {
        /* Purely non-blocking */
        ret = 0;
    }

exit:
    os_mutex_unlock(dev->buffer_mutex);
    return ret;
}


int  usbapi_pollout(usbapi_device *dev,int msecs)
{
    int ret;

    if(!dev){
        LOGD(TAG,"Invalid parameter!");
        return -1;
    }

    if(dev->handle==INVALID_HANDLE_VALUE){
        LOGD(TAG,"Invalid handle!");
        return -1;
    }

    if(!dev->info->output_endpoint){
        LOGE(TAG,"Output endpoint not exist!");
        return -1;
    }

    os_select(INVALID_HANDLE_VALUE,dev->handle,msecs,ret);

    return ret;
}

const usbapi_device_info* usbapi_getinfo(usbapi_device*dev)
{
    if(!dev)
        return NULL;
    return dev->info;
}
