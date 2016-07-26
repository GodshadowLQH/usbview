#ifndef LINUX_NETLINK_H
#define LINUX_NETLINK_H

typedef void (*linux_plugin_cb)(int bus,int dev,const char* sys_name);
typedef void (*linux_plugout_cb)(int bus,int dev,const char* sys_name);

int linux_netlink_start_event_monitor(linux_plugin_cb in_cb,linux_plugout_cb out_cb);
int linux_netlink_stop_event_monitor(void);

#endif // LINUX_NETLINK_H
