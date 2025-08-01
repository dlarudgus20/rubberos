#include "inst.h"

extern inline void out8(uint16_t port, uint8_t data);
extern inline uint8_t in8(uint16_t port);
extern inline void wait_for_int(void);
