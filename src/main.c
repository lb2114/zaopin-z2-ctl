#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <systemd/sd-device.h>

#define INTERFACE_ID 1
#define VID 0x3554
#define PID 0xf527

#define NO_MOUSE 2


int open_hidraw(int *fd) {
    sd_device_enumerator *enumerator = NULL;
    sd_device *hidraw_device;
    int r;

    r = sd_device_enumerator_new(&enumerator);
    if (r < 0) {
        fprintf(stderr, "enumerator: %s\n", strerror(-r));
        return 1;
    }

    r = sd_device_enumerator_add_match_subsystem(enumerator, "hidraw", 1);
    if (r < 0) {
        fprintf(stderr, "match subsys: %s\n", strerror(-r));
        sd_device_enumerator_unref(enumerator);
        return 1;
    }

    for (hidraw_device= sd_device_enumerator_get_device_first(enumerator); hidraw_device != NULL; hidraw_device = sd_device_enumerator_get_device_next(enumerator)) {
        // check usb interface first, because in protocol it sends messages to first one
        sd_device *usb_interface;
        r = sd_device_get_parent_with_subsystem_devtype(hidraw_device, "usb", "usb_interface", &usb_interface);
        if (r < 0) {
            fprintf(stderr, "interface: %s\n", strerror(-r)); 
            sd_device_enumerator_unref(enumerator);
            return -1;
        }
        const char *num;
        int r = sd_device_get_sysattr_value(usb_interface, "bInterfaceNumber", &num);
        if (r < 0) {
            fprintf(stderr, "Interface: %s\n", strerror(-r)); 
            sd_device_enumerator_unref(enumerator);
            return -1;
        }
        long num_val = strtol(num, NULL, 16);
        if (num_val != INTERFACE_ID) {
            continue;
        }

        sd_device *usb_device;
        r = sd_device_get_parent_with_subsystem_devtype(hidraw_device, "usb", "usb_device", &usb_device);
        if (r < 0) {
            fprintf(stderr, "get usb device: %s\n", strerror(-r)); 
            sd_device_enumerator_unref(enumerator);
            return -1;
        }
        const char *vid, *pid;
        if (sd_device_get_sysattr_value(usb_device, "idVendor", &vid) < 0 ||
                sd_device_get_sysattr_value(usb_device, "idProduct", &pid) < 0) { 
            sd_device_enumerator_unref(enumerator);
            return -1;
        }
        long vid_val = strtol(vid, NULL, vid_val);
        long pid_val = strtol(pid, NULL, pid_val);
        if (vid_val == VID && pid_val == PID) {
            break;
        }
    }

    if (hidraw_device == NULL) {
        return NO_MOUSE;
    }

    const char *path;
    r = sd_device_get_devname(hidraw_device, &path);
    if (r < 0) {
        fprintf(stderr, "devname: %s\n", strerror(-r));
        sd_device_enumerator_unref(enumerator);
        return -1;
    }

    r = open(path, O_RDWR);
    if (r) {
        *fd = r;
        fprintf(stderr, "open: %s\n", strerror(r));
        sd_device_enumerator_unref(enumerator);
        return -1; 
    }
 
    sd_device_enumerator_unref(enumerator);
    return 0;
}


int main() {
    int fd;
    open_hidraw(&fd);
    return 0;
}

