/*
Copyright 2019 Tomoaki Itoh
This software is released under the MIT License, see LICENSE.txt.
//*/

#include <stdint.h>

#define LCW_OSC_TABLE_BITS (10)
#define LCW_OSC_TABLE_SIZE (1 << LCW_OSC_TABLE_BITS)
#define LCW_OSC_TABLE_MASK ((LCW_OSC_TABLE_SIZE) - 1)
#define LCW_OSC_SUB_TABLE_SIZE (5)

#define LCW_OSC_SHORT_CUT_TABLE_BITS (10)
#define LCW_OSC_SHORT_CUT_TABLE_SIZE (1 << LCW_OSC_SHORT_CUT_TABLE_BITS)
#define LCW_OSC_SHORT_CUT_LUT_OFFSET (2.0)

// SQ1.14
typedef int16_t LCWOscWaveTable[LCW_OSC_TABLE_SIZE];

typedef struct {
    const int16_t fn;
    const int16_t pitch;
    const int16_t gain;
} LCWOscWaveOption;

typedef LCWOscWaveOption LCWOscWaveSubTable[LCW_OSC_SUB_TABLE_SIZE];

typedef struct {
    int32_t count;
    const LCWOscWaveTable *tables;
    const LCWOscWaveSubTable *subTables;
    const int8_t *shortCutTable;
} LCWOscWaveSource;

extern const LCWOscWaveSource gLcwOscPulseSource;

