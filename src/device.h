#define READ 0x08
#define WRITE 0x07
#define BATTERY 0x04


int get_active_profile(int fd, int *profile);

int device_show_profile(int fd);
int device_set_profile(int fd, int n);

int device_show_profiles(int fd);
int device_set_profiles(int fd, int n);

int device_show_dpi(int fd);
int device_set_dpi(int fd, int dpi);

int device_show_freq(int fd);
int device_set_freq(int fd, int hz);

int device_show_sleep(int fd);
int device_set_sleep(int fd, int seconds);

int device_show_battery(int fd);
