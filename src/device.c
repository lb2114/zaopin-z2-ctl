#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/hidraw.h>

#include "device.h"
#include "protocol.h"


static int send_report(int fd, uint8_t report[REPORT_SIZE]) {
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


static int read_report(int fd, uint8_t report[REPORT_SIZE]) {
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


static int exchange_report(int fd, uint8_t rep[REPORT_SIZE], uint8_t cmd, uint8_t addr, uint8_t size, uint8_t* data) {
    if ((cmd != READ && cmd != WRITE && cmd != BATTERY) || size > 0x0A) {
        fprintf(stderr, "invalid cmd idx\n");
        return -1;
    }
    memset(rep, 0, REPORT_SIZE);
    rep[0] = 0x08;
    rep[1] = cmd;
    rep[4] = addr;
    rep[5] = size;
    if (cmd == WRITE) {
        if (data == NULL) {
            fprintf(stderr, "Null data pointer\n");
            return -1;
        }
        memcpy(&rep[6], data, size);
    }
    add_checksums(rep);

    if (send_report(fd, rep)) {
        return -1;
    }
    if (cmd == READ || cmd == BATTERY) {
        if (read_report(fd, rep)) {
            return -1;
        }
        if (cmd != BATTERY && (rep[4] != addr || rep[5] != size)) {
            fprintf(stderr, "Unexpected response address or size\n");
            return -1;
        }
        if (check_response(rep)) {
            fprintf(stderr, "Invalid report\n");
            return -1;
        }
    }

    return 0;
}


static void print_time(int sec) {
    int min = sec / 60;
    sec %= 60;

    if (min > 0) {
        printf("%dm", min);
    }

    if (sec > 0 || min == 0) {
        printf("%s%ds", min > 0 ? " " : "", sec);
    }

    putchar('\n');
}


int get_active_profile(int fd, int* profile) {
    *profile = -1;
    uint8_t rep[REPORT_SIZE] = {0}; 
    if (exchange_report(fd, rep, READ, 0x04, 0x02, NULL)) {
        return -1;
    }
    
    *profile = (int)rep[6];
    return 0;
}

int device_show_profile(int fd) {
    int profile;
    if (get_active_profile(fd, &profile)) {
        return -1;
    }
    printf("%d\n", profile + 1);
    return 0;
}

int device_set_profile(int fd, int n) {
    uint8_t rep[REPORT_SIZE];
    if (exchange_report(fd, rep, READ, 0x02, 0x02, NULL)) {
        return -1;
    }
    int max_profiles = (int)rep[6];
    if (n > max_profiles || n < 1) {
        fprintf(stderr, "Invalid profile\n");
        return -1;
    }
    
    uint8_t data[2] = {(uint8_t)n - 1, 0};
    if (exchange_report(fd, rep, WRITE, 0x04, 0x02, data)) {
        return -1;
    }
    return 0;
}

int device_show_profiles(int fd) {
    uint8_t rep[REPORT_SIZE];
    if (exchange_report(fd, rep, READ, 0x02, 0x02, NULL)) {
        return -1;
    }
    int p_count = (int)rep[6];
    for (int i = 0; i < p_count; i++) {
        uint8_t addr = 0x0c + 4 * i;
        if (exchange_report(fd, rep, READ, addr, 0x04, NULL)) {
            return -1;
        }
        int dpi;
        if (decode_dpi(rep, &dpi)) {
            return -1;
        }
        printf("Profile %d: %d dpi\n", i + 1, dpi);
    }
    return 0;
}

int device_set_profiles(int fd, int n) {
    if (n < 1 || n > 6) {
        return -1;
    }
    uint8_t rep[REPORT_SIZE];
    uint8_t data[2] = {(uint8_t)n, 0};
    if (exchange_report(fd, rep, WRITE, 0x02, 0x02, data)) {
        return -1;
    }
    return 0;
}

int device_show_dpi(int fd) {
    int profile_idx;
    if (get_active_profile(fd, &profile_idx)) {
        fprintf(stderr, "Fail to read profile\n");
        return -1;
    }
    uint8_t addr = 0x0c + 4 * profile_idx;
    uint8_t report[REPORT_SIZE] = {0};
    if (exchange_report(fd, report, READ, addr, 0x04, NULL)) {
        return -1;
    }
    int dpi;
    if (decode_dpi(report, &dpi)) {
        return -1;
    }
    printf("%d\n", dpi);

    return 0;
}

int device_set_dpi(int fd, int dpi) {
    int profile_idx;
    if (get_active_profile(fd, &profile_idx)) {
        fprintf(stderr, "Fail to read profile\n");
        return -1;
    }
    uint8_t data[4];
    if (encode_dpi(data, dpi)) {
        fprintf(stderr, "Invalid value\n");
        return -1;
    }
    uint8_t addr = 0x0c + 4 * profile_idx;
    uint8_t report[REPORT_SIZE] = {0};
    if (exchange_report(fd, report, WRITE, addr, 0x04, data)) {
        return -1;
    } 
    
    return 0;
}

int device_show_freq(int fd) {
    int val;
    uint8_t report[REPORT_SIZE];
    if (exchange_report(fd, report, READ, 0x00, 0x02, NULL)) {
       return -1; 
    }
    val = decode_freq(report[6]); 
    if (val == -1) {
        fprintf(stderr, "Incorrect value\n");
        return -1;
    }
    printf("%dHz\n", val);
    return 0;
}

int device_set_freq(int fd, int hz) {
    uint8_t report[REPORT_SIZE];
    uint8_t data[2];
    if (encode_freq(data, hz)) {
        fprintf(stderr, "Invalid value\n");
        return -1;
    }
    if (exchange_report(fd, report, WRITE, 0x00, 0x02, data)) {
        return -1;
    }
    return 0;
}


int device_show_sleep(int fd) {
    uint8_t report[REPORT_SIZE];
    if (exchange_report(fd, report, READ, 0xAD, 0x02, NULL)) {
        return -1;
    }
    int sec = decode_sleep(report[6]);
    if (sec == -1) {
        fprintf(stderr, "Invalid value\n");
        return -1;
    }
    print_time(sec);
    return 0;
}

int device_set_sleep(int fd, int seconds) {
    uint8_t report[REPORT_SIZE];
    uint8_t data[2];
    if (encode_sleep(data, seconds)) {
        fprintf(stderr, "Invalid value\n");
        return -1;
    }
    if (exchange_report(fd, report, WRITE, 0xAD, 0x02, data)) {
        return -1;
    }
    return 0;
}

int device_show_battery(int fd) {
    uint8_t report[REPORT_SIZE];
    if (exchange_report(fd, report, BATTERY, 0x00, 0x00, NULL)) {
        return -1;
    }
    if (report[7]) {
        printf("Charging\n");
    }
    printf("%d%%\n", (int)(report[6]));
    return 0;
}
