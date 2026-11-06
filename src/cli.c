#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#include "cli.h"
#include "device.h"




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

int parse_args(int argc, char *argv[],
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

    if (parse_int(argv[3], &value) != 0) {
        return -1;
    }

    switch (out->command) {
    case CMD_PROFILE:
        if (value < 1 || value > 6) {
            return -1;
        }
        break;

    case CMD_PROFILES:
        if (value < 1 || value > 6) {
            return -1;
        }
        break;

    case CMD_DPI:
        if (value < 50 || value > 26000 ||
            value % 50 != 0) {
            return -1;
        }
        break;

    case CMD_FREQ:
        if (value != 125 && value != 250 &&
            value != 500 && value != 1000 &&
            value != 2000 && value != 4000) {
            return -1;
        }
        break;

    case CMD_TIMETOSLEEP:
        if (value < 10 || value > 2400 ||
            value % 10 != 0) {
            return -1;
        }
        break;

    case CMD_BATTERY:
        return -1;
    }

    out->action = ACTION_SET;
    out->value = value;

    return 0;
}

int execute_command(const struct parsed_args *args, int fd) {
    switch (args->command) {
    case CMD_PROFILE:
        return args->action == ACTION_SHOW
            ? device_show_profile(fd)
            : device_set_profile(fd, args->value);

    case CMD_PROFILES:
        return args->action == ACTION_SHOW
            ? device_show_profiles(fd)
            : device_set_profiles(fd, args->value);

    case CMD_DPI:
        return args->action == ACTION_SHOW
            ? device_show_dpi(fd)
            : device_set_dpi(fd, args->value);

    case CMD_FREQ:
        return args->action == ACTION_SHOW
            ? device_show_freq(fd)
            : device_set_freq(fd, args->value);

    case CMD_TIMETOSLEEP:
        return args->action == ACTION_SHOW
            ? device_show_sleep(fd)
            : device_set_sleep(fd, args->value);

    case CMD_BATTERY:
        return device_show_battery(fd);
    }

    return -1;
}

void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s profile [show|set <1..6>]\n"
        "  %s profiles [show|set <1..6>]\n"
        "  %s dpi [show|set <50..26000, step 50>]\n"
        "  %s freq [show|set <125|250|500|1000|2000|4000>]\n"
        "  %s timetosleep [show|set <10..2400 seconds, step 10>]\n"
        "  %s battery\n",
        prog, prog, prog, prog, prog, prog);
}
