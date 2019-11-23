# The ArduboyTonesDotMG Library

ArduboyTonesDotMG is a library for playing tones and tone sequences, intended to be used with the Modmatic dotMG handheld game system.

## Description

**ArduboyTonesDotMG** is a small, efficient library which generates tones and tone sequences on a speaker attached to digital output pins. It is intended to be used by games and other sketches written for the dotMG, so the pins (and timer) used are *hard coded* for the dotMG, and thus aren't specified in any functions.

Like the [Arduino built in *tone()* function](https://www.arduino.cc/en/Reference/Tone), the tones are simple square waves generated by toggling a pin digitally high and low with a 50% duty cycle. Also like Arduino *tone()*, once started, interrupts are used so that the sound plays in the background while the *real* code continues to run.

ArduboyTonesDotMG has equivalents to Arduino [*tone()*](https://www.arduino.cc/en/Reference/Tone) and [*noTone()*](https://www.arduino.cc/en/Reference/NoTone) and additionally:

- As well as a single tone, the *tone()* function can play two or three tones, in sequence, with a single call.
- Includes functions to play a tone sequence of any length, specified by an array located in either program memory ([PROGMEM](https://www.arduino.cc/en/Reference/PROGMEM)) or RAM. The array can be optionally terminated with a *repeat* command so the sequence will repeat continuously unless stopped by using *noTone()* or a new tone or sequence is started.
- Each tone can specify that it's to be played at either normal or a higher volume. High volume is accomplished by taking advantage of the speaker being wired across two pins and toggling each pin opposite to the other, which will generate twice the normal voltage across the speaker. For normal volume, one pin is toggled while the other is held low.
- A function is available to set flags to ignore the individual volume setting in each tone, so that all tones will play at either normal or high volume.
- Tone sequences can include intervals of silence (musical rests).
- Includes a global *mute* capability by using a callback function, specified by the sketch, which indicates sound is to be muted. When muted, the sketch can continue to call functions to produce tones in the usual way but the speaker will remain silent. It is intended that the Arduboy2DotMG Library's *audio.enabled()* function, or something similar, be used as this function.
- A function is provided which will return `true` if tones are playing or `false` if the sequence has completed.

Note that even with all these features, ArduboyTonesDotMG will use significantly less code space than using Arduino *tone()* functions.

### Converting MIDI files

A separate command line utility program, [midi2tones](https://github.com/MLXXXp/midi2tones), is available. It will convert [standard MIDI files](https://www.midi.org/articles/about-midi-part-4-midi-files) to ArduboyTones format when the `-o2` option is specified.

## Installation

1. Download this repo as a ZIP file to a known location on your machine.

2. Install the library using the following guide:

[Installing Additional Arduino Libraries](https://www.arduino.cc/en/Guide/Libraries#toc5)

## Using the library

### Setting up

Include the library using

```cpp
#include <ArduboyTones.h>
```

You must then create an *ArduboyTones* object which specifies the callback function used for muting. The function is a required parameter. It must return a *boolean* (or *bool*) value which will be `true` if sound should be played, or `false` if all sounds should be muted. In this document the *ArduboyTones* object will be named *sound*. The *audio.enabled()* function of the *Arduboy* library will be used for the mute callback. The *Arduboy* object will be named *arduboy*.

So, to set things up we can use:

```cpp
#include <Arduboy2.h>
#include <ArduboyTones.h>

Arduboy2 arduboy;
ArduboyTones sound(arduboy.audio.enabled);
```

### Specifying tones

Each tone is specified by a frequency and duration pair, the same as with Arduino *tone()*. Like *tone()*, the frequency is specified in hertz (Hz, cycles per second). The duration is in milliseconds (actually very slightly shorter; see below).

There are some small differences between the Arduino *tone()* and the ArduboyTones frequency and duration parameters.

#### Frequency

- ArduboyTones can only play frequencies between 16 Hz and 32767 Hz. Arduino *tone()* allows a greater range.
- For efficiency, ArduboyTones uses a single clock prescaler value for the timer. The result is that very high frequencies may be a bit less accurate than what Arduino *tone()* would produce. It likely won't be noticeable for the range of frequencies that will mainly be used.
- With ArduboyTones, you can use a frequency value of 0 to indicate silence (a musical rest) for the duration specified.
- You can indicate that a tone should sound at a higher volume by adding the defined value *TONE_HIGH_VOLUME* to the desired frequency.

#### Duration

- To make the code smaller and more efficient, durations are actually in 1024ths of a second instead of 1000ths (milliseconds). See the note near the end of this document for a detailed explanation.
- With ArduboyTones, a tone's duration is specified as an unsigned int (16 bits) instead of an unsigned long (32 bits). This means the maximum duration allowed is 65535 (65 seconds). Like Arduino *tone()* a duration of 0 indicates forever, which can be stopped by *noTone()* or replaced by a new tone or sequence call.

#### Defines for musical notes

 ArduboyTones includes file *ArduboyTonesPitches.h* which will automatically be included by *ArduboyTones.h*. *ArduboyTonesPitches.h* provides frequency values for musical notes based on A4 = 440 Hz.

The format is:

`NOTE_<letter><optional S for sharp><octave number><optional H for high volume>`

*NOTE_REST* is defined as 0 and can be used for a musical rest.

There are no flats. You have to use the sharp note equivalent.

Notes are define over the range of C0 (16 Hz) to B9 (15804 Hz)

Examples: `NOTE_C3 NOTE_GS4 NOTE_B5H NOTE_DS6H`

### Functions

All functions are members of class *ArduboyTones*, so you must remember to reference each function with the object name.
Example: `sound.tone(1000, 500);`

----------

Play a tone forever or until interrupted:

`void tone(frequency)`

Play a single tone for the duration. Duration can be 0 (forever):

`void tone(frequency, duration)`

Play two tones in sequence:

`void tone(freq1, dur1, freq2, dur2)`

Play three tones in sequence:

`void tone(freq1, dur1, freq2, dur2, freq3, dur3)`

----------

Play a tone sequence from frequency/duration pairs in an array in program memory:

`void tones(arrayInProgram)`

The array must end with a single value of either *TONES_END*, to end the sequence, or *TONES_REPEAT* to continuously repeat from beginning to end.

Example:

```cpp
const uint16_t song1[] PROGMEM = {
  220,1000, 0,250, 440,500, 880,2000,
  TONES_END };

sound.tones(song1);
```

----------

The same as *tones()* above except the frequency/duration pairs are in an array in RAM instead of in program memory:

`void tonesInRAM(arrayInRAM)`

 Generally, the only reason to use *tonesInRAM()* instead of *tones()* would be if dynamically altering the contents of the array is required.

Example:

```cpp
uint16_t song2[] = {
  NOTE_A3,1000, NOTE_REST,250, NOTE_A4,500, NOTE_A5,2000,
  TONES_REPEAT };

sound.tonesInRAM(song2);
```

----------

Stop playing the tone or sequence:

`void noTone()`

If called when nothing is playing, it will do nothing.

----------

Set the volume to always be normal, always high, or tone controlled:

`void volumeMode(mode)`

The following values for *mode* should be used:

- *VOLUME_IN_TONE* - Each tone will play at the volume specified in the tone itself. This is the default.
- *VOLUME_ALWAYS_NORMAL* - All tones will play at normal volume level regardless of what's specified in the tones.
- *VOLUME_ALWAYS_HIGH* - All tones will play at high volume level regardless of what's specified in the tones.

----------

Check if a tone or tone sequence is playing:

`boolean playing()`

Returns `true` if playing (even if sound is muted).

----------

### Notes and Hints

#### Example sketch

The ArduboyTones library contains an example sketch *ArduboyTonesTest* in the *examples* folder. It was primarily written to test the library but the code can be examined and uploaded for examples of using all the functions. It uses the [*Arduboy2*](https://github.com/MLXXXp/Arduboy2) library, which must be installed to compile the sketch. *ArduboyTonesTest* can be loaded into the Arduino IDE using the menus:

`File > Examples > ArduboyTones > ArduboyTonesTest`

#### Frequencies and durations work the same everywhere

You can use rests and infinite durations in *tone()* functions the same as in sequence arrays. Most of the time this wouldn't be useful, however...

- A rest would only be useful in the middle of a three tone *tone()* function, to play two tones with a rest between them. Or maybe on the end, if you were chaining *tone()* functions by using *playing()* to wait for each one to end, and then starting the next one immediately.
- A duration of 0 (forever) is only useful on the last tone of a sequence, even in an array.
- You can use *TONES_REPEAT* (and even *TONES_END*) in place of a frequency in a *tone()* function. In this case the mandatory duration value would be ignored. About the only place this might be useful would be using *TONES_REPEAT* at the end of a three tone *tone()* function, to play two tones repeatedly. For instance, two tones repeated could be used when a character starts walking and then *noTone()* could be called when the character stopped.

Something to consider for all of the above points, though:

Using a two or three tone array in program memory with *tones()* is likely more efficient than using a two or three tone *tone()* function. The two and three tone *tone()* functions were only added to make things easier and quick for sketches where code size isn't an issue.

#### Why durations aren't exactly in milliseconds

Ideally, to match Arduino *tone()*, durations should be given in 1000ths of a second (milliseconds). However, ArduboyTones treats durations as being in 1024ths of a second. Here's why:

To calculate internal timing values for a duration given in milliseconds, a divide by 500 on an *unsigned long* (32 bit) number is required, which is what Arduino *tone()* does. On an 8 bit processor without any native divide instructions, this is slow and takes a fair amount of code. On the other hand, a divide by 512 is easily and quickly accomplished by simply shifting the value right 9 bits. This is what ArduboyTones does, at the expense of durations being about 2.34% shorter than the same value would be with Arduino *tone()*.

In most circumstances, the slightly shorter durations will likely be unnoticeable. If a duration needs to be precise, the required value can be calculated by multiplying the desired duration, in milliseconds, by 1.024.
