#include <stdint.h>

#define REPORT_SIZE 17

void add_checksums(uint8_t response[REPORT_SIZE]);

int decode_freq(uint8_t code);

int encode_freq(uint8_t data[2], int hz);

int encode_sleep(uint8_t data[2], int seconds); 

int decode_sleep(uint8_t val);

int check_response(uint8_t report[REPORT_SIZE]); 

// 2 bytes for value (dpi = val * 50), 1 byte for multiplicator (0x00, 0x44, 0x88)
// this protocol appears to support per-axis DPI configuration, but I haven't checked if the mouse supports
int decode_dpi(const uint8_t report[REPORT_SIZE], int *dpi);

int encode_dpi(uint8_t data[4], int dpi); 
