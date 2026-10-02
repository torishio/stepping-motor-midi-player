/*
 * stp_motr_channel_velocity.ino
 *
 * Copyright (c) 2026 HCN TORISHIO
 * Released under the MIT License (see LICENSE in the repository root).
 *
 * Variant of stp_motr_channel.ino that also maps MIDI velocity to
 * perceived "volume". Everything else (pin map, USB MIDI, singing-stepper
 * pitch mechanism, stall-avoidance microstepping) is identical -- see that
 * sketch's header comment for the full explanation. This file only adds
 * the velocity -> loudness piece described below.
 *
 * ------------------------------------------------------------------
 * REQUIRED Arduino IDE SETUP (read this before flashing):
 * ------------------------------------------------------------------
 *   1. Library: install "MIDIUSB" (by Gary Grewal) via
 *      Sketch > Include Library > Manage Libraries.
 *   2. Board: this board is a Pro Micro-COMPATIBLE clone, not a
 *      genuine Arduino Micro -- it has no D11/D12/D13, and its
 *      silkscreen A0-A3 / D14-D16 are wired differently than the
 *      real Arduino Micro's A0-A5. Selecting "Arduino Micro" in the
 *      Boards menu will map these pin names to the WRONG physical
 *      pins. Select a Pro Micro (ATmega32u4, 5V/16MHz) board profile
 *      instead (e.g. SparkFun AVR Boards > SparkFun Pro Micro,
 *      installable via Boards Manager if not already present).
 *   3. Wiring reference: see hardware/stp-motr-schematic -- STEP=D4,
 *      DIR=D5, ENABLE=D6, MS1=A0, MS2=A1, MS3=A2.
 * ------------------------------------------------------------------
 *
 * Pitch mechanism: the STEP pin is toggled at exactly the target
 * note's frequency (the classic "singing stepper" trick). Timer1
 * (hardware CTC interrupt) drives the pulses so USB polling in loop()
 * never causes timing jitter.
 *
 * Velocity -> "volume": the A4988 has no amplitude/PWM output to control
 * here -- a stepper either steps or it doesn't, so there's no volume knob
 * in the usual sense. What DOES change the perceived loudness is the
 * MICROSTEP MODE: full/half step moves the coil current in big, abrupt
 * jumps (louder, more percussive "buzz"), while finer microstepping
 * (1/8, 1/16) moves it in small smooth increments (quieter, softer tone).
 * So velocity is mapped onto that same axis this sketch already uses for
 * stall avoidance -- velocityToModeIndex() turns velocity 127 into the
 * coarsest/loudest mode and velocity 1 into the finest/quietest one.
 *
 * That request and the EXISTING stall-avoidance request (from pitch) can
 * disagree -- e.g. a hard hit (wants coarse/loud) on a high note (needs
 * fine, to avoid stalling). Stall avoidance always wins: selectStepMode()
 * takes whichever of the two is the FINER mode, so velocity can only make
 * a note quieter than the pitch would otherwise allow, never coarser (and
 * therefore never introduces a stall the plain pitch-only version wouldn't
 * already have avoided). In practice this means loud low notes buzz the
 * loudest, and anything high-pitched or soft trends quieter -- both real
 * pianos and this driver-current-modulation trick behave that way anyway.
 *
 * Debug output: open the Arduino IDE's Serial Monitor (115200 baud) to see
 * which note is currently sounding, its frequency, incoming velocity, and
 * the microstep mode that resulted -- updated every time a note starts,
 * stops, or changes. Separate USB Serial port from the MIDI connection, so
 * it works whether or not anything is listening -- no need to disconnect
 * from a DAW.
 */

#include <MIDIUSB.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

// ---- Pin map (Pro Micro-compatible silkscreen labels) --------------------
const uint8_t PIN_STEP = 4;
const uint8_t PIN_DIR = 5;
const uint8_t PIN_ENABLE = 6;
const uint8_t PIN_MS1 = A0;
const uint8_t PIN_MS2 = A1;
const uint8_t PIN_MS3 = A2;

// ---- Motor / driver tuning -------------------------------------------------
#define MAX_STEP_RATE 400.0f   // mechanical (full-step-equivalent) steps/sec
                                // ceiling. Start here, raise gradually (see
                                // the original sketch's header for how).

// A4988 microstep table: {MS1, MS2, MS3, multiplier}. Ordered coarsest
// (loudest) first -- both the stall-avoidance search and the velocity
// mapping below rely on that order.
struct StepMode {
  uint8_t ms1, ms2, ms3;
  uint8_t multiplier;
};
const StepMode STEP_MODES[] = {
  {0, 0, 0, 1},   // full step     -- loudest
  {1, 0, 0, 2},   // half step
  {0, 1, 0, 4},   // quarter step
  {1, 1, 0, 8},   // eighth step
  {1, 1, 1, 16},  // sixteenth step -- quietest
};
const uint8_t N_STEP_MODES = sizeof(STEP_MODES) / sizeof(STEP_MODES[0]);

// ---- MIDI channel filter ---------------------------------------------------
#define MIDI_CHANNEL_FILTER 0

// ---- Simple monophonic note stack (last-note priority with fallback) ------
// Now tracks each held note's velocity alongside it, so releasing the top
// note reveals both the note AND the velocity it was originally struck with.
#define NOTE_STACK_SIZE 8
uint8_t noteStack[NOTE_STACK_SIZE];
uint8_t velocityStack[NOTE_STACK_SIZE];
uint8_t noteStackLen = 0;

void pushNote(uint8_t note, uint8_t velocity) {
  for (uint8_t i = 0; i < noteStackLen; i++) {
    if (noteStack[i] == note) {
      velocityStack[i] = velocity;  // re-struck while held -- refresh velocity
      return;
    }
  }
  if (noteStackLen < NOTE_STACK_SIZE) {
    noteStack[noteStackLen] = note;
    velocityStack[noteStackLen] = velocity;
    noteStackLen++;
  } else {
    // stack full: drop the oldest, shift up, append newest
    memmove(noteStack, noteStack + 1, NOTE_STACK_SIZE - 1);
    memmove(velocityStack, velocityStack + 1, NOTE_STACK_SIZE - 1);
    noteStack[NOTE_STACK_SIZE - 1] = note;
    velocityStack[NOTE_STACK_SIZE - 1] = velocity;
  }
}

void removeNote(uint8_t note) {
  for (uint8_t i = 0; i < noteStackLen; i++) {
    if (noteStack[i] == note) {
      memmove(noteStack + i, noteStack + i + 1, noteStackLen - i - 1);
      memmove(velocityStack + i, velocityStack + i + 1, noteStackLen - i - 1);
      noteStackLen--;
      return;
    }
  }
}

// ---- Step mode selection ---------------------------------------------------
void applyStepMode(const StepMode &m) {
  digitalWrite(PIN_MS1, m.ms1);
  digitalWrite(PIN_MS2, m.ms2);
  digitalWrite(PIN_MS3, m.ms3);
}

// velocity 127 -> index 0 (coarsest/loudest), velocity 1 -> index
// N_STEP_MODES-1 (finest/quietest), linear in between. Divide by 126 (not
// 127): velocity's real range here is [1,127] since velocity 0 is a Note
// Off, not a quiet Note On -- dividing by the full 127 would leave
// velocity=1 one step short of the finest mode.
uint8_t velocityToModeIndex(uint8_t velocity) {
  uint16_t idx = (uint16_t)(127 - velocity) * (N_STEP_MODES - 1) / 126;
  if (idx >= N_STEP_MODES) idx = N_STEP_MODES - 1;
  return (uint8_t)idx;
}

const StepMode &selectStepMode(float freqHz, uint8_t velocity) {
  // Coarsest mode that still keeps the mechanical rate stall-safe.
  uint8_t stallSafeIdx = N_STEP_MODES - 1;
  for (uint8_t i = 0; i < N_STEP_MODES; i++) {
    float mechanicalRate = freqHz / STEP_MODES[i].multiplier;
    if (mechanicalRate <= MAX_STEP_RATE) {
      stallSafeIdx = i;
      break;
    }
  }
  uint8_t velocityIdx = velocityToModeIndex(velocity);
  // Whichever asks for the FINER (larger-index) mode wins -- velocity can
  // only make a note quieter than stall-avoidance allows, never coarser.
  uint8_t idx = (stallSafeIdx > velocityIdx) ? stallSafeIdx : velocityIdx;
  return STEP_MODES[idx];
}

// ---- Timer1: hardware-clocked STEP pulse generator -------------------------
// CTC mode, toggles PIN_STEP on every compare match at exactly `freqHz`.
volatile bool stepPinState = false;

void stopStepping() {
  TIMSK1 &= ~(1 << OCIE1A);
  digitalWrite(PIN_STEP, LOW);
  stepPinState = false;
}

void setStepFrequency(float freqHz) {
  if (freqHz <= 0.5f) {
    stopStepping();
    return;
  }

  const uint16_t prescalers[] = {1, 8, 64, 256, 1024};
  const uint8_t csBits[] = {
    (1 << CS10),
    (1 << CS11),
    (1 << CS11) | (1 << CS10),
    (1 << CS12),
    (1 << CS12) | (1 << CS10),
  };

  for (uint8_t i = 0; i < 5; i++) {
    // toggling twice per cycle -> compare-match interrupt fires at 2x freqHz
    uint32_t ocr = (uint32_t)(F_CPU / (2UL * prescalers[i] * freqHz)) - 1;
    if (ocr <= 65535UL) {
      noInterrupts();
      TCCR1A = 0;
      TCCR1B = (1 << WGM12) | csBits[i];  // CTC, TOP = OCR1A
      OCR1A = (uint16_t)ocr;
      TCNT1 = 0;
      TIMSK1 |= (1 << OCIE1A);
      interrupts();
      return;
    }
  }
  // frequency too low even at the largest prescaler -- stop rather than
  // wrap around into a bogus (huge) OCR1A value.
  stopStepping();
}

ISR(TIMER1_COMPA_vect) {
  stepPinState = !stepPinState;
  digitalWrite(PIN_STEP, stepPinState ? HIGH : LOW);
}

// ---- Note -> frequency ------------------------------------------------------
float noteToFrequency(uint8_t note) {
  return 440.0f * pow(2.0f, (note - 69) / 12.0f);
}

// ---- Note -> human-readable name (e.g. "A4", "C#5") for the debug log ------
void noteName(uint8_t note, char *out) {
  static const char *NAMES[12] = {"C", "C#", "D", "D#", "E", "F",
                                   "F#", "G", "G#", "A", "A#", "B"};
  int8_t octave = (int8_t)(note / 12) - 1;
  const char *name = NAMES[note % 12];
  sprintf(out, "%s%d", name, octave);
}

// ---- Debug output: what's currently sounding, and how ----------------------
void logVoice(bool playing, uint8_t note, float freq, uint8_t velocity, const StepMode &mode) {
  if (!playing) {
    Serial.println(F("[voice] off"));
    return;
  }
  char name[5];
  noteName(note, name);
  float mechanicalRate = freq / mode.multiplier;
  Serial.print(F("[voice] note="));
  Serial.print(note);
  Serial.print(' ');
  Serial.print(name);
  Serial.print(F("  vel="));
  Serial.print(velocity);
  Serial.print(F("  freq="));
  Serial.print(freq, 1);
  Serial.print(F("Hz  microstep=1/"));
  Serial.print(mode.multiplier);
  Serial.print(F("  mech_rate="));
  Serial.print(mechanicalRate, 1);
  Serial.println(F("steps/s"));
}

// ---- Apply whatever note is now on top of the stack -------------------------
void updateVoice() {
  if (noteStackLen == 0) {
    stopStepping();
    logVoice(false, 0, 0, 0, STEP_MODES[0]);
    return;
  }
  uint8_t note = noteStack[noteStackLen - 1];
  uint8_t velocity = velocityStack[noteStackLen - 1];
  float freq = noteToFrequency(note);
  const StepMode &mode = selectStepMode(freq, velocity);
  applyStepMode(mode);
  setStepFrequency(freq);
  logVoice(true, note, freq, velocity, mode);
}

// ---- MIDI handling -----------------------------------------------------------
void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
#if MIDI_CHANNEL_FILTER != 0
  if (channel != (MIDI_CHANNEL_FILTER - 1)) return;
#endif
  if (velocity == 0) {
    removeNote(note);  // MIDI convention: NoteOn vel=0 == NoteOff
  } else {
    pushNote(note, velocity);
  }
  updateVoice();
}

void handleNoteOff(uint8_t channel, uint8_t note) {
#if MIDI_CHANNEL_FILTER != 0
  if (channel != (MIDI_CHANNEL_FILTER - 1)) return;
#endif
  removeNote(note);
  updateVoice();
}

void pollMidi() {
  midiEventPacket_t rx;
  do {
    rx = MidiUSB.read();
    switch (rx.header) {
      case 0x9:  // Note On
        {
          uint8_t status = rx.byte1;
          uint8_t channel = status & 0x0F;
          uint8_t note = rx.byte2;
          uint8_t velocity = rx.byte3;
          handleNoteOn(channel, note, velocity);
        }
        break;
      case 0x8:  // Note Off
        {
          uint8_t status = rx.byte1;
          uint8_t channel = status & 0x0F;
          uint8_t note = rx.byte2;
          handleNoteOff(channel, note);
        }
        break;
      default:
        break;
    }
  } while (rx.header != 0);
}

// ---- Setup / loop -------------------------------------------------------------
void setup() {
  Serial.begin(115200);  // debug log only -- MIDI itself goes over MIDIUSB,
                          // not this port, so nothing needs the DAW/Serial
                          // Monitor to be listening for notes to work.

  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ENABLE, OUTPUT);
  pinMode(PIN_MS1, OUTPUT);
  pinMode(PIN_MS2, OUTPUT);
  pinMode(PIN_MS3, OUTPUT);

  digitalWrite(PIN_ENABLE, LOW);  // A4988 ENABLE is active-low; left enabled
                                  // permanently (see schematic design notes)
  digitalWrite(PIN_DIR, HIGH);    // continuous one-direction spin -- this is
                                  // a tone generator, not a positioner
  digitalWrite(PIN_STEP, LOW);
  applyStepMode(STEP_MODES[0]);
}

void loop() {
  pollMidi();
}
