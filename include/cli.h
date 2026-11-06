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

int execute_command(const struct parsed_args *args, int fd);

int parse_args(int argc, char *argv[], struct parsed_args *out);
   
void print_usage(const char *prog);

