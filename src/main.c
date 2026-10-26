#include <stdio.h>
#include <stdlib.h>

#include "hid.h"
#include "cli.h"


int main(int argc, char *argv[]) {
    int fd;
    if (open_hidraw(&fd)) {
        fprintf(stderr, "Fail to open mouse to write\n");
        return -1;
    }

    struct parsed_args args;

    if (parse_args(argc, argv, &args) != 0) {
        fprintf(stderr, "Ivalid command\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (execute_command(&args, fd) != 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

