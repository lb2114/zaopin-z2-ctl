#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <systemd/sd-device.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/hidraw.h>

#define INTERFACE_ID 1
#define VID 0x3554
#define PID 0xf527
#define REPORT_SIZE 17

#define NO_MOUSE 2

#define READ_OP 8
#define WRITE_OP 7


enum command {
    CMD_PROFILE,       // активный профиль: 0..5
    CMD_PROFILES,      // число доступных профилей: 1..6
    CMD_DPI,
    CMD_FREQ,
    CMD_TIMETOSLEEP,
    CMD_BATTERY
};

enum action {
    ACTION_SHOW,
    ACTION_SET
};

struct parsed_args {
    enum command command;
    enum action action;
    int value;          // для timetosleep — секунды
};


void add_checksums(uint8_t response[REPORT_SIZE]) {
    if (response[1] == 0x07) {
        if (response[5] == 2) {
            response[7] = (uint8_t)(0x55 - response[6]);
        } else if (response[5] == 4) {
            response[9] = (uint8_t)(0x55 - response[6]
                                         - response[7]
                                         - response[8]);
        }
    }

    uint8_t checksum = 0x55;

    for (int i = 0; i < REPORT_SIZE - 1; i++) {
        checksum -= response[i];
    }

    response[REPORT_SIZE - 1] = checksum;
}



int send_report(int fd, uint8_t report[REPORT_SIZE]) {
    int r = ioctl(fd, HIDIOCSOUTPUT(REPORT_SIZE), report);

    if (r < 0) {
        perror("HIDIOCSOUTPUT");
        return -1;
    }

    if (r != REPORT_SIZE) {
        fprintf(stderr, "Unexpected report size: %d\n", r);
        return -1;
    }

    return 0;
}

int read_report(int fd, uint8_t report[REPORT_SIZE]) {
    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN
    };

    int r = poll(&pfd, 1, 1000);

    if (r < 0) {
        perror("poll");
        return -1;
    }

    if (r == 0) {
        fprintf(stderr, "Mouse not responding (possibly sleeping, try to move it)\n");
        return -1;
    }

    if (!(pfd.revents & POLLIN)) {
        fprintf(stderr, "Unexpected poll event: 0x%x\n", pfd.revents);
        return -1;
    }

    ssize_t n = read(fd, report, REPORT_SIZE);

    if (n != REPORT_SIZE) {
        perror("read");
        return -1;
    }

    return 0;
}

int check_response(uint8_t report[REPORT_SIZE]) {
    if (report[0] != 0x08 ||
            report[1] != 0x08) {
        return -1;
    }
    uint8_t sum = 0;
    for (int i = 0; i < REPORT_SIZE; i++) {
        sum += report[i];
    }
    if (sum != 0x55) {
        return -1;
    }
    return 0;
}


static int parse_int(const char *text, int *result) {
    char *end;
    errno = 0;
    long n = strtol(text, &end, 10);

    if (errno == ERANGE || end == text || *end != '\0' ||
        n < INT_MIN || n > INT_MAX) {
        return -1;
    }

    *result = (int)n;
    return 0;
}

static int parse_sleep_time(const char *text, int *seconds) {
    char *end;
    errno = 0;
    long n = strtol(text, &end, 10);

    if (errno == ERANGE || end == text ||
        end[0] == '\0' || end[1] != '\0') {
        return -1;
    }

    if (end[0] == 's' &&
        n >= 10 && n <= 60 && n % 10 == 0) {
        *seconds = (int)n;
        return 0;
    }

    if (end[0] == 'm' && n >= 1 && n <= 20) {
        *seconds = (int)(n * 60);
        return 0;
    }

    return -1;
}

static int parse_args(int argc, char *argv[],
                      struct parsed_args *out) {
    if (out == NULL || argc < 2) {
        return -1;
    }

    if (strcmp(argv[1], "profile") == 0) {
        out->command = CMD_PROFILE;
    } else if (strcmp(argv[1], "profiles") == 0) {
        out->command = CMD_PROFILES;
    } else if (strcmp(argv[1], "dpi") == 0) {
        out->command = CMD_DPI;
    } else if (strcmp(argv[1], "freq") == 0) {
        out->command = CMD_FREQ;
    } else if (strcmp(argv[1], "timetosleep") == 0) {
        out->command = CMD_TIMETOSLEEP;
    } else if (strcmp(argv[1], "battery") == 0) {
        out->command = CMD_BATTERY;
    } else {
        return -1;
    }

    if (out->command == CMD_BATTERY) {
        if (argc != 2) {
            return -1;
        }

        out->action = ACTION_SHOW;
        out->value = 0;
        return 0;
    }

    if (argc == 2 ||
        (argc == 3 && strcmp(argv[2], "show") == 0)) {
        out->action = ACTION_SHOW;
        out->value = 0;
        return 0;
    }

    if (argc != 4 || strcmp(argv[2], "set") != 0) {
        return -1;
    }

    int value;

    if (out->command == CMD_TIMETOSLEEP) {
        if (parse_sleep_time(argv[3], &value) != 0) {
            return -1;
        }
    } else if (parse_int(argv[3], &value) != 0) {
        return -1;
    }

    switch (out->command) {
    case CMD_PROFILE:
        if (value < 0 || value > 5) {
            return -1;
        }
        break;

    case CMD_PROFILES:
        if (value < 1 || value > 6) {
            return -1;
        }
        break;

    case CMD_DPI:
        if (value < 400 || value > 26000 ||
            (value - 400) % 50 != 0) {
            return -1;
        }
        break;

    case CMD_FREQ:
        /*
         * Подтверждены: 125, 500, 1000, 2000 Гц.
         * 4000 Гц добавлены, но поддержка не проверена.
         * 250 Гц также не проверены.
         *
         * TODO: проверить по протоколу возможность
         * установки произвольных значений.
         */
        if (value != 125 && value != 500 &&
            value != 1000 && value != 2000 &&
            value != 4000) {
            return -1;
        }
        break;

    case CMD_TIMETOSLEEP:
        break;

    case CMD_BATTERY:
        return -1;
    }

    out->action = ACTION_SET;
    out->value = value;

    return 0;
}


static int get_active_profile(int fd, int* profile) {
    //0x04 - addr, 0x02 - bytes to read
    *profile = -1;
    uint8_t rep[REPORT_SIZE] = {0}; 
    rep[0] = 0x08;
    rep[1] = READ_OP;
    rep[4] = 0x04; //addr
    rep[5] = 0x02; //bytes to read
    add_checksums(rep);
    
    if (send_report(fd, rep)) {
        return -1;
    }

    if (read_report(fd, rep)) {
        return -1;
    }
    if (check_response(rep)) {
        fprintf(stderr, "Invalid report\n");
        return -1;
    }
    
    *profile = (int)rep[6];
    return 0;
}

static int device_show_profile(int fd) {
    int profile;
    if (get_active_profile(fd, &profile)) {
        return -1;
    }
    printf("%d\n", profile);
    return 0;
}

static int device_set_profile(int n) {
    // TODO: проверить n < текущего количества профилей.
    printf("[stub] set active profile to %d\n", n);
    return 0;
}

static int device_show_profiles(void) {
    puts("[stub] show profile count");
    return 0;
}

static int device_set_profiles(int n) {
    /*
     * Уменьшение числа профилей не должно стирать
     * сохранённые значения DPI.
     */
    printf("[stub] set profile count to %d\n", n);
    return 0;
}

static int device_show_dpi(int fd) {
    puts("[stub] show DPI");
    return 0;
}

static int device_set_dpi(int n) {
    printf("[stub] set DPI to %d\n", n);
    return 0;
}

static int device_show_freq(void) {
    puts("[stub] show polling rate");
    return 0;
}

static int device_set_freq(int n) {
    printf("[stub] set polling rate to %d Hz\n", n);
    return 0;
}

static int device_show_sleep(void) {
    puts("[stub] show sleep time");
    return 0;
}

static int device_set_sleep(int seconds) {
    printf("[stub] set sleep time to %d seconds\n", seconds);
    return 0;
}

static int device_show_battery(void) {
    puts("[stub] show battery");
    return 0;
}

static int execute_command(const struct parsed_args *args, int fd) {
    switch (args->command) {
    case CMD_PROFILE:
        return args->action == ACTION_SHOW
            ? device_show_profile(fd)
            : device_set_profile(args->value);

    case CMD_PROFILES:
        return args->action == ACTION_SHOW
            ? device_show_profiles()
            : device_set_profiles(args->value);

    case CMD_DPI:
        return args->action == ACTION_SHOW
            ? device_show_dpi(fd)
            : device_set_dpi(args->value);

    case CMD_FREQ:
        return args->action == ACTION_SHOW
            ? device_show_freq()
            : device_set_freq(args->value);

    case CMD_TIMETOSLEEP:
        return args->action == ACTION_SHOW
            ? device_show_sleep()
            : device_set_sleep(args->value);

    case CMD_BATTERY:
        return device_show_battery();
    }

    return -1;
}

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s profile [show|set <0..5>]\n"
        "  %s profiles [show|set <1..6>]\n"
        "  %s dpi [show|set <400..26000, step 50>]\n"
        "  %s freq [show|set <125|500|1000|2000|4000>]\n"
        "  %s timetosleep [show|set <10s|...|60s|1m|...|20m>]\n"
        "  %s battery\n",
        prog, prog, prog, prog, prog, prog);
}



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
        long vid_val = strtol(vid, NULL, 16);
        long pid_val = strtol(pid, NULL, 16);
        if (vid_val == VID && pid_val == PID) {
            break;
        }
    }

    if (hidraw_device == NULL) {
        sd_device_enumerator_unref(enumerator);
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
    *fd = r;
    if (r == -1) {
        fprintf(stderr, "open: %s\n", strerror(errno));
        sd_device_enumerator_unref(enumerator);
        return -1; 
    }

 
    sd_device_enumerator_unref(enumerator);
    return 0;
}


int main(int argc, char *argv[]) {
    int fd;
    if (open_hidraw(&fd)) {
        fprintf(stderr, "Fail to open mouse to write\n");
        return -1;
    }

    struct parsed_args args;

    if (parse_args(argc, argv, &args) != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (execute_command(&args, fd) != 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

