/**
 * @file ArduboyTones.h
 * \brief An Arduino library for playing tones and tone sequences,
 * intended for the Modmatic dotMG game system.
 */

#ifndef ARDUBOY_TONES_H
#define ARDUBOY_TONES_H

#include <Arduino.h>

// ************************************************************
// ***** Values to use as function parameters in sketches *****
// ************************************************************

/** \brief
 * Frequency value for sequence termination. (No duration follows)
 */
#define TONES_END 0x8000

/** \brief
 * Frequency value for sequence repeat. (No duration follows)
 */
#define TONES_REPEAT 0x8001


/** \brief
 * Add this to the frequency to play a tone at high volume
 */
#define TONE_HIGH_VOLUME 0x8000


/** \brief
 * `volumeMode()` parameter. Use the volume encoded in each tone's frequency
 */
#define VOLUME_IN_TONE 0

/** \brief
 * `volumeMode()` parameter. Play all tones at normal volume, ignoring
 * what's encoded in the frequencies
 */
#define VOLUME_ALWAYS_NORMAL 1

/** \brief
 * `volumeMode()` parameter. Play all tones at high volume, ignoring
 * what's encoded in the frequencies
 */
#define VOLUME_ALWAYS_HIGH 2

// This must match the maximum number of tones that can be specified in
// the tone() function.
#define MAX_TONES 3

#define PIN_SPEAKER    PIN_DAC1
#define DAC_CH_SPEAKER 1
#define DAC_READY      DAC->STATUS.bit.READY1
#define DAC_DATA_BUSY  DAC->SYNCBUSY.bit.DATA1

// Change these if there's a conflict with timers between this library
// and others
#define TIMER_CTRL    TC3
#define TIMER_GCLK_ID TC3_GCLK_ID
#define TIMER_IRQ     TC3_IRQn
#define TIMER_HANDLER void TC3_Handler()

// Dummy frequency used to for silent tones (rests).
#define SILENT_FREQ 25


/** \brief
 * The ArduboyTones class for generating tones by specifying
 * frequency/duration pairs.
 */
class ArduboyTones
{
 public:
  /** \brief
   * The ArduboyTones class constructor.
   *
   * \param outEn A function which returns a boolean value of `true` if sound
   * should be played or `false` if sound should be muted. This function will
   * be called from the timer interrupt service routine, at the start of each
   * tone, so it should be as fast as possible.
   */
  ArduboyTones(bool (*outEn)());

  /** \brief
   * Play a single tone.
   *
   * \param freq The frequency of the tone, in hertz.
   * \param dur The duration to play the tone for, in 1024ths of a
   * second (very close to milliseconds). A duration of 0, or if not provided,
   * means play forever, or until `noTone()` is called or a new tone or
   * sequence is started.
   */
  static void tone(uint16_t freq, uint16_t dur = 0);

  /** \brief
   * Play two tones in sequence.
   *
   * \param freq1,freq2 The frequency of the tone in hertz.
   * \param dur1,dur2 The duration to play the tone for, in 1024ths of a
   * second (very close to milliseconds).
   */
  static void tone(uint16_t freq1, uint16_t dur1,
                   uint16_t freq2, uint16_t dur2);

  /** \brief
   * Play three tones in sequence.
   *
   * \param freq1,freq2,freq3 The frequency of the tone, in hertz.
   * \param dur1,dur2,dur3 The duration to play the tone for, in 1024ths of a
   * second (very close to milliseconds).
   */
  static void tone(uint16_t freq1, uint16_t dur1,
                   uint16_t freq2, uint16_t dur2,
                   uint16_t freq3, uint16_t dur3);

  /** \brief
   * Play a tone sequence from frequency/duration pairs in a PROGMEM array.
   *
   * \param tones A pointer to an array of frequency/duration pairs.
   * The array must be placed in code space using `PROGMEM`.
   *
   * \details
   * \parblock
   * See the `tone()` function for details on the frequency and duration values.
   * A frequency of 0 for any tone means silence (a musical rest).
   *
   * The last element of the array must be `TONES_END` or `TONES_REPEAT`.
   *
   * Example:
   *
   * \code
   * const uint16_t sound1[] PROGMEM = {
   *   220,1000, 0,250, 440,500, 880,2000,
   *   TONES_END
   * };
   * \endcode
   *
   * \endparblock
   */
  static void tones(const uint16_t *tones);

  /** \brief
   * Play a tone sequence from frequency/duration pairs in an array in RAM.
   *
   * \param tones A pointer to an array of frequency/duration pairs.
   * The array must be located in RAM.
   *
   * \see tones()
   *
   * \details
   * \parblock
   * See the `tone()` function for details on the frequency and duration values.
   * A frequency of 0 for any tone means silence (a musical rest).
   *
   * The last element of the array must be `TONES_END` or `TONES_REPEAT`.
   *
   * Example:
   *
   * \code
   * uint16_t sound2[] = {
   *   220,1000, 0,250, 440,500, 880,2000,
   *   TONES_END
   * };
   * \endcode
   *
   * \endparblock
   *
   * \note Using `tones()`, with the data in PROGMEM, is normally a better
   * choice. The only reason to use tonesInRAM() would be if dynamically
   * altering the contents of the array is required.
   */
  static void tonesInRAM(uint16_t *tones);

  /** \brief
   * Stop playing the tone or sequence.
   *
   * \details
   * If a tone or sequence is playing, it will stop. If nothing
   * is playing, this function will do nothing.
   */
  static void noTone();

  /** \brief
   * Originally intended to set the volume to always normal, always high, or tone controlled.
   * For dotMG, this method has no effect as volume will always be normal.
   *
   * \param mode
   * \parblock
   * Originally, this would be one of the following values:
   *
   * - `VOLUME_IN_TONE` The volume of each tone will be specified in the tone
   *    itself.
   * - `VOLUME_ALWAYS_NORMAL` All tones will play at the normal volume level.
   * - `VOLUME_ALWAYS_HIGH` All tones will play at the high volume level.
   *
   * However, as noted in the method description, volume will always be normal for dotMG,
   * regardless of calls to this method.
   * \endparblock
   */
  static void volumeMode(uint8_t mode);

  /** \brief
   * Check if a tone or tone sequence is playing.
   *
   * \return boolean `true` if playing (even if sound is muted).
   */
  static bool playing();

private:
  // Get the next value in the sequence
  static uint16_t getNext();

public:
  // Called from ISR so must be public. Should not be called by a program.
  static void nextTone();
};

#include "ArduboyTonesPitches.h"

#endif
