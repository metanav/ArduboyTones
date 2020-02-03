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


ArduboyTones::ArduboyTones(boolean (*outEn)())
{
  outputEnabled = outEn;

  toneSequence[MAX_TONES * 2] = TONES_END;

  // Enable GCLK0 for timer
  GCLK->CLKCTRL.reg =
    GCLK_CLKCTRL_CLKEN |
    GCLK_CLKCTRL_GEN_GCLK0 |
    GCLK_CLKCTRL_ID(CLKCTRL_ID);
  while (GCLK->STATUS.bit.SYNCBUSY);

  // Reset timer
  TIMER_CTRL->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
  while (TIMER_CTRL->COUNT16.STATUS.bit.SYNCBUSY);
  while (TIMER_CTRL->COUNT16.CTRLA.bit.SWRST);

  // Set to 16-bit counter, match frequency mode, clk/16 prescaler
  TIMER_CTRL->COUNT16.CTRLA.reg = (
    TC_CTRLA_MODE_COUNT16 |
    TC_CTRLA_WAVEGEN_MFRQ |
    TC_CTRLA_PRESCALER_DIV16
  );
  while (TIMER_CTRL->COUNT16.STATUS.bit.SYNCBUSY);

  // Configure interrupt request
  NVIC_DisableIRQ(TIMER_IRQ);
  NVIC_ClearPendingIRQ(TIMER_IRQ);
  NVIC_SetPriority(TIMER_IRQ, 0);
  NVIC_EnableIRQ(TIMER_IRQ);

  // Enable interrupt request
  TIMER_CTRL->COUNT16.INTENSET.bit.MC0 = 1;
  while (TIMER_CTRL->COUNT16.STATUS.bit.SYNCBUSY);
}

void ArduboyTones::tone(uint16_t freq, uint16_t dur)
{
  // Disable counter
  TIMER_CTRL->COUNT16.CTRLA.bit.ENABLE = 0;
  while (TIMER_CTRL->COUNT16.STATUS.bit.SYNCBUSY);

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
  // Disable counter
  TIMER_CTRL->COUNT16.CTRLA.bit.ENABLE = 0;
  while (TIMER_CTRL->COUNT16.STATUS.bit.SYNCBUSY);

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
  // Disable counter
  TIMER_CTRL->COUNT16.CTRLA.bit.ENABLE = 0;
  while (TIMER_CTRL->COUNT16.STATUS.bit.SYNCBUSY);

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
  // Disable counter
  TIMER_CTRL->COUNT16.CTRLA.bit.ENABLE = 0;
  while (TIMER_CTRL->COUNT16.STATUS.bit.SYNCBUSY);

  inProgmem = true;
  tonesStart = tonesIndex = (uint16_t *)tones; // set to start of sequence array
  nextTone(); // start playing
}

void ArduboyTones::tonesInRAM(uint16_t *tones)
{
  // Disable counter
  TIMER_CTRL->COUNT16.CTRLA.bit.ENABLE = 0;
  while (TIMER_CTRL->COUNT16.STATUS.bit.SYNCBUSY);

  inProgmem = false;
  tonesStart = tonesIndex = tones; // set to start of sequence array
  nextTone(); // start playing
}

void ArduboyTones::noTone()
{
  // Disable counter
  TIMER_CTRL->COUNT16.CTRLA.bit.ENABLE = 0;
  while (TIMER_CTRL->COUNT16.STATUS.bit.SYNCBUSY);

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
    // bitClear(TONE_PIN_PORT, TONE_PIN); // set the pin low
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

  // Enable counter
  TIMER_CTRL->COUNT16.CTRLA.bit.ENABLE = 1;
  while (TIMER_CTRL->COUNT16.STATUS.bit.SYNCBUSY);
}

uint16_t ArduboyTones::getNext()
{
  if (inProgmem) {
    return pgm_read_word(tonesIndex++);
  }
  return *tonesIndex++;
}

TIMER_HANDLER
{
  if (durationToggleCount != 0) {
    if (!toneSilent) {
      DAC->DATA.reg = DAC->DATA.reg ? 0 : (toneHighVol ? 1023 : 511);
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
