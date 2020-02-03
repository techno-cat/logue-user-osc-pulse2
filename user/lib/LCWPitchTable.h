/*
Copyright 2019 Tomoaki Itoh
This software is released under the MIT License, see LICENSE.txt.
//*/

#include <stdint.h>

#define LCW_PITCH_DELTA_VALUE_BITS (28)
#define LCW_NOTE_NO_A4 (69) // Note No. @ 440Hz

#ifdef __cplusplus
extern "C" {
#endif

// pitch s15.16
// return u4.28
extern uint32_t pitch_to_timer_delta(int32_t pitch);

#ifdef __cplusplus
}
#endif 
