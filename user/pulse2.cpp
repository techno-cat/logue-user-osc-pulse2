/*
Copyright 2019 Tomoaki Itoh
This software is released under the MIT License, see LICENSE.txt.
//*/

#include "userosc.h"
#include "LCWCommon.h"
#include "LCWPitchTable.h"
#include "LCWAntiAliasingTable.h"
#include "LCWOscWaveSource.h"

#define LCW_OSC_TIMER_BITS (LCW_PITCH_DELTA_VALUE_BITS)
#define LCW_OSC_TIMER_MAX (1 << LCW_OSC_TIMER_BITS)
#define LCW_OSC_TIMER_MASK ((LCW_OSC_TIMER_MAX) - 1)

static struct {
  float shape = 0;
  float shiftshape = 0;
} s_param;

static struct {
  uint32_t timer1 = 0;
  uint32_t timer2 = 0;
  int32_t pitch1 = 0;   // s7.24
  int32_t pitch2 = 0;   // s7.24
  int32_t shape_lfo = 0;
} s_state;

// pitch/return : s7.24
__fast_inline int32_t pitchLimitter(int32_t pitch)
{
#if 0
  if ( LCW_SQ7_24(6.0) < pitch ) {
    // 1:8
    return LCW_SQ7_24(5.6) + (pitch - (LCW_SQ7_24(6.0)) >> 3);
  }
  else if ( LCW_SQ7_24(5.6) < pitch ) {
    // 1:4
    return LCW_SQ7_24(5.5) + (pitch - (LCW_SQ7_24(5.6)) >> 2);
  }
  else if ( LCW_SQ7_24(5.4) < pitch ) {
    // 1:2
    return LCW_SQ7_24(5.4) + (pitch - (LCW_SQ7_24(5.4)) >> 1);
  }
  else {
    return pitch;
  }
#else
// 2^5.1502 * 440 = 31250 * 0.5(Hz)
  if ( LCW_SQ7_24(5.5) < pitch ) {
    // 1:8
    return LCW_SQ7_24(5.1);// + (pitch - (LCW_SQ7_24(5.5)) >> 3);
  }
  else if ( LCW_SQ7_24(5.1) < pitch ) {
    // 1:4
    return LCW_SQ7_24(5.0) + (pitch - (LCW_SQ7_24(5.1)) >> 2);
  }
  else if ( LCW_SQ7_24(4.9) < pitch ) {
    // 1:2
    return LCW_SQ7_24(4.9) + (pitch - (LCW_SQ7_24(4.9)) >> 1);
  }
  else {
    return pitch;
  }
#endif
}

// テーブル選択
__fast_inline int32_t mySelectTable(
  const LCWOscWaveSource *src, int32_t pitch)
{
  pitch += LCW_SQ7_24(LCW_OSC_SHORT_CUT_LUT_OFFSET);
  if ( pitch < 0 ) {
    return src->shortCutTable[0];
  }

  const int32_t i = pitch >> (24 - 7); // = 整数部 / 128
  return ( LCW_OSC_SHORT_CUT_TABLE_SIZE <= i )
    ? 0
    : src->shortCutTable[i];
}

#define LCW_AA_NYQUIST_PITCH (5224) // = 5.1 * 2^10, 2^5.1 * 440 = 15090(Hz)
// pitch : s7.24, return : SQ.22
__fast_inline int32_t myLookUp(
  const LCWOscWaveSource *src, uint32_t t, int32_t pitch, int32_t i)
{
  // AAテーブルを参照するための添字に加工
  const int32_t j0 = (pitch >> (24 - LCW_AA_TABLE_BITS)) - LCW_AA_PITCH_OFFSET;
  // 10bit線形補間の準備
  const uint32_t t0 = t >> (LCW_OSC_TIMER_BITS - (LCW_OSC_TABLE_BITS + 10));

  int64_t out = 0; // = SQ.25 x AA
  {
    const int16_t *p = src->tables[i];

    int32_t j = ( j0 < 0 ) ? 0 : j0;
    if ( LCW_AA_TABLE_SIZE <= j ) {
      j = LCW_AA_TABLE_SIZE - 1;
    }

    int32_t gain = gLcwAntiAiliasingTable[j];
    uint32_t t1 = t0 >> 10;
    uint32_t t2 = (t1 + 1) & LCW_OSC_TABLE_MASK;

    int32_t ratio = t0 & 0x3FF;
    int32_t val = (p[t1] * ratio) + (p[t2] * (0x400 - ratio));
    out += ( (int64_t)val * gain );
  }

  const LCWOscWaveOption *options = src->subTables[i];
  for (int32_t k=0; k<LCW_OSC_SUB_TABLE_SIZE; k++) {
    const int16_t *p = src->tables[0]; // sin wave
    const LCWOscWaveOption *option = options + k;
#if(1)
    // memo: pitchの小数部はAAテーブルのサイズに合わせてある
    int32_t j = j0 + option->pitch;
    if ( j < (LCW_AA_NYQUIST_PITCH - LCW_AA_PITCH_OFFSET) ) {
      j = ( j < 0 ) ? 0 : j;
      int32_t gain = (gLcwAntiAiliasingTable[j] * option->gain) >> LCW_OSC_SUB_TABLE_GAIN_BITS;
      uint32_t tt = t0 * option->fn;
      uint32_t t1 = (tt >> 10) & LCW_OSC_TABLE_MASK;
      uint32_t t2 = (t1 + 1) & LCW_OSC_TABLE_MASK;

      int32_t ratio = tt & 0x3FF;
      int32_t val = (p[t1] * ratio) + (p[t2] * (0x400 - ratio));
      out += ( (int64_t)val * gain );
    }
#endif
  }

  return (int32_t)( out >> (LCW_AA_VALUE_BITS + (25 - 22)) );
}

void OSC_INIT(uint32_t platform, uint32_t api)
{
  s_param.shape = 0.f;
  s_param.shiftshape = 0.f;

  s_state.timer1 = 0;
  s_state.timer2 = 0;
  s_state.pitch1 =
  s_state.pitch2 = (LCW_NOTE_NO_A4 << 24) / 12;
  s_state.shape_lfo = 0;
}

void OSC_CYCLE(const user_osc_param_t * const params,
               int32_t *yn,
               const uint32_t frames)
{
  // s11.20に拡張してから、整数部がoctaveになるように加工
  int32_t pitch1 = (int32_t)params->pitch << 12;
  pitch1 = (pitch1 - (LCW_NOTE_NO_A4 << 20)) / 12;
  const int32_t note = (int32_t)si_roundf(24.f * s_param.shape);
  // [0 .. 24] -> [-12 .. +12]
  int32_t pitch2 = pitch1 + (((note - 12) << 20) / 12);

  int32_t lfo_delta = (params->shape_lfo - s_state.shape_lfo) / (int32_t)frames;

  // s11.20 -> s7.24
  pitch1 <<= 4;
  pitch2 <<= 4;

  // Temporaries.
  uint32_t t1 = s_state.timer1;
  uint32_t t2 = s_state.timer2;
  int32_t shape_lfo = s_state.shape_lfo;

  q31_t * __restrict y = (q31_t *)yn;
  const q31_t * y_e = y + frames;

  // Main Mix/Sub Mix, 8bit(= [0-256])
  const int32_t subVol = (int32_t)(0x100 * s_param.shiftshape);
  const int32_t mainVol = 0x100 - subVol;

  const LCWOscWaveSource *src = &gLcwOscPulseSource;
  for (; y != y_e; ) {
    // q22
    int32_t out1 = myLookUp(
      src, t1, pitch1, mySelectTable(src, pitch1) );
    int32_t out2 = myLookUp(
      src, t2, pitch2, mySelectTable(src, pitch2) );

    // q22 -> q30
    int32_t out = (out1 * mainVol) + (out2 * subVol);

    // q30 -> q31
    *(y++) = (q31_t)(out << (31 - 30));

    // input: s7.24 -> s15.16
    uint32_t dt1 = pitch_to_timer_delta( pitchLimitter(pitch1) >> 8 );
    uint32_t dt2 = pitch_to_timer_delta( pitchLimitter(pitch2) >> 8 );
    t1 = (t1 + dt1) & LCW_OSC_TIMER_MASK;
    t2 = (t2 + dt2) & LCW_OSC_TIMER_MASK;
  
    shape_lfo += lfo_delta;
    pitch2 += (shape_lfo >> 16);
  }

  s_state.timer1 = t1;
  s_state.timer2 = t2;
  s_state.shape_lfo = params->shape_lfo;
  s_state.pitch1 = pitch1;
  s_state.pitch2 = pitch2;
}

void OSC_NOTEON(const user_osc_param_t * const params)
{
  return;
}

void OSC_NOTEOFF(const user_osc_param_t * const params)
{
  return;
}

void OSC_PARAM(uint16_t index, uint16_t value)
{
  float valf = param_val_to_f32( value );
  switch (index) {
  case k_user_osc_param_shape:
    s_param.shape = clip01f( valf );
    break;
  case k_user_osc_param_shiftshape:
    s_param.shiftshape = clip01f( valf );
    break;
  default:
    break;
  }
}
