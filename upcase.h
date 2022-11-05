#ifndef upcase_h
#define upcase_h
#include <stdint.h>
uint16_t toUpcase(uint16_t chr);
uint32_t upcaseChecksum(uint16_t unicode, uint32_t checksum);
#endif  // upcase_h