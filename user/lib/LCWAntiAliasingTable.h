#include <stdint.h>

#define LCW_AA_PITCH_OFFSET (0x1333) // 4.8

#define LCW_AA_TABLE_BITS (10)
#define LCW_AA_TABLE_SIZE (1024)

#define LCW_AA_VALUE_BITS (14)
#define LCW_AA_VALUE_MAX (1 << (LCW_AA_VALUE_BITS))

extern const int16_t gLcwAntiAiliasingTable[LCW_AA_TABLE_SIZE];

