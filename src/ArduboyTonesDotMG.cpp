/**
 * @file ArduboyTonesDotMG.cpp
 * \brief An Arduino library for playing tones and tone sequences,
 * intended for the Modmatic dotMG game system.
 */

#include "ArduboyTonesDotMG.h"

// pointer to a function that indicates if sound is enabled
static bool (*outputEnabled)();

static volatile long durationToggleCount = 0;
static volatile bool tonesPlaying = false;
static volatile bool toneSilent;
static volatile bool toneHighVol;
static volatile bool forceHighVol = false;
static volatile bool forceNormVol = false;

static volatile uint16_t *tonesStart;
static volatile uint16_t *tonesIndex;
static volatile uint16_t toneSequence[MAX_TONES * 2 + 1];
static volatile bool inProgmem;

void enable_counter(boolean enable)
{
  TIMER_CTRL->COUNT16.CTRLA.bit.ENABLE = enable;
  while (TIMER_CTRL->COUNT16.SYNCBUSY.bit.ENABLE);
}

ArduboyTones::ArduboyTones(boolean (*outEn)())
{
  outputEnabled = outEn;

  toneSequence[MAX_TONES * 2] = TONES_END;

  // Enable GCLK for timer
  GCLK->PCHCTRL[TIMER_GCLK_ID].reg = GCLK_PCHCTRL_GEN_GCLK0_Val | (1 << GCLK_PCHCTRL_CHEN_Pos);

  // Disable counter
  enable_counter(false);

  // Reset counter
  TIMER_CTRL->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
  while (TIMER_CTRL->COUNT16.SYNCBUSY.bit.ENABLE);
  while (TIMER_CTRL->COUNT16.CTRLA.bit.SWRST);

  // Set to match frequency mode
  TIMER_CTRL->COUNT16.WAVE.reg = TC_WAVE_WAVEGEN_MFRQ;

  // Set to 16-bit counter, clk/16 prescaler
  TIMER_CTRL->COUNT16.CTRLA.reg = (
    TC_CTRLA_MODE_COUNT16 |
    TC_CTRLA_PRESCALER_DIV16
  );
  while (TIMER_CTRL->COUNT16.SYNCBUSY.bit.ENABLE);

  // Configure interrupt request
  NVIC_DisableIRQ(TIMER_IRQ);
  NVIC_ClearPendingIRQ(TIMER_IRQ);
  NVIC_SetPriority(TIMER_IRQ, 0);
  NVIC_EnableIRQ(TIMER_IRQ);

  // Enable interrupt request
  TIMER_CTRL->COUNT16.INTENSET.bit.MC0 = 1;
  while (TIMER_CTRL->COUNT16.SYNCBUSY.bit.ENABLE);
}

void ArduboyTones::tone(uint16_t freq, uint16_t dur)
{
  enable_counter(false);

  inProgmem = false;
  tonesStart = tonesIndex = toneSequence; // set to start of sequence array
  toneSequence[0] = freq;
  toneSequence[1] = dur;
  toneSequence[2] = TONES_END; // set end marker
  nextTone(); // start playing
}

void ArduboyTones::tone(uint16_t freq1, uint16_t dur1,
                        uint16_t freq2, uint16_t dur2)
{
  enable_counter(false);

  inProgmem = false;
  tonesStart = tonesIndex = toneSequence; // set to start of sequence array
  toneSequence[0] = freq1;
  toneSequence[1] = dur1;
  toneSequence[2] = freq2;
  toneSequence[3] = dur2;
  toneSequence[4] = TONES_END; // set end marker
  nextTone(); // start playing
}

void ArduboyTones::tone(uint16_t freq1, uint16_t dur1,
                        uint16_t freq2, uint16_t dur2,
                        uint16_t freq3, uint16_t dur3)
{
  enable_counter(false);

  inProgmem = false;
  tonesStart = tonesIndex = toneSequence; // set to start of sequence array
  toneSequence[0] = freq1;
  toneSequence[1] = dur1;
  toneSequence[2] = freq2;
  toneSequence[3] = dur2;
  toneSequence[4] = freq3;
  toneSequence[5] = dur3;
  // end marker was set in the constructor and will never change
  nextTone(); // start playing
}

void ArduboyTones::tones(const uint16_t *tones)
{
  enable_counter(false);

  inProgmem = true;
  tonesStart = tonesIndex = (uint16_t *)tones; // set to start of sequence array
  nextTone(); // start playing
}

void ArduboyTones::tonesInRAM(uint16_t *tones)
{
  enable_counter(false);

  inProgmem = false;
  tonesStart = tonesIndex = tones; // set to start of sequence array
  nextTone(); // start playing
}

void ArduboyTones::noTone()
{
  enable_counter(false);
  tonesPlaying = false;
}

void ArduboyTones::volumeMode(uint8_t mode)
{
  forceNormVol = false; // assume volume is tone controlled
  forceHighVol = false;

  if (mode == VOLUME_ALWAYS_NORMAL) {
    forceNormVol = true;
  }
  else if (mode == VOLUME_ALWAYS_HIGH) {
    forceHighVol = true;
  }
}

bool ArduboyTones::playing()
{
  return tonesPlaying;
}

void ArduboyTones::nextTone()
{
  uint16_t freq;
  uint16_t dur;
  long toggleCount;
  uint32_t timerCount;

  freq = getNext(); // get tone frequency

  if (freq == TONES_END) { // if freq is actually an "end of sequence" marker
    noTone(); // stop playing
    return;
  }

  tonesPlaying = true;

  if (freq == TONES_REPEAT) { // if frequency is actually a "repeat" marker
    tonesIndex = tonesStart; // reset to start of sequence
    freq = getNext();
  }

  if (((freq & TONE_HIGH_VOLUME) || forceHighVol) && !forceNormVol) {
    toneHighVol = true;
  }
  else {
    toneHighVol = false;
  }

  freq &= ~TONE_HIGH_VOLUME; // strip volume indicator from frequency

  if (freq == 0) { // if tone is silent
    timerCount = F_CPU / 16 / SILENT_FREQ / 2 - 1; // dummy tone for silence
    freq = SILENT_FREQ;
    toneSilent = true;
  }
  else {
    timerCount = F_CPU / 16 / freq / 2 - 1;
    toneSilent = false;
  }

  if (!outputEnabled()) { // if sound has been muted
    toneSilent = true;
  }

  dur = getNext(); // get tone duration
  if (dur != 0) {
    // A right shift is used to divide by 512 for efficency.
    // For durations in milliseconds it should actually be a divide by 500,
    // so durations will by shorter by 2.34% of what is specified.
    toggleCount = ((long)dur * freq) >> 9;
  }
  else {
    toggleCount = -1; // indicate infinite duration
  }

  durationToggleCount = toggleCount;

  // Set counter based on desired frequency
  TIMER_CTRL->COUNT16.CC[0].reg = timerCount;
  enable_counter(true);
}

uint16_t ArduboyTones::getNext()
{
  if (inProgmem) {
    return pgm_read_word(tonesIndex++);
  }
  return *tonesIndex++;
}

volatile bool val;
TIMER_HANDLER
{
  if (durationToggleCount != 0) {
    if (!toneSilent) {
      if (!DAC->DACCTRL[DAC_CH_SPEAKER].bit.ENABLE)
        return;

      while (!DAC_READY);
      while (DAC_DATA_BUSY);
      val = !val;
      DAC->DATA[DAC_CH_SPEAKER].reg = val ? 0 : (toneHighVol ? 4095 : 3072);
    }

    if (durationToggleCount > 0) {
      durationToggleCount--;
    }
  }
  else {
    ArduboyTones::nextTone();
  }

  // Clear the interrupt
  TIMER_CTRL->COUNT16.INTFLAG.bit.MC0 = 1;
}
