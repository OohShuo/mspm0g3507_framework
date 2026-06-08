#pragma once
#include <stdint.h>

/**
 * synth.h — Polyphonic software synthesizer for PWM buzzer.
 *
 * Architecture:
 *   - 4-voice polyphonic mixer
 *   - Per-voice oscillator (phase accumulator + wavetable)
 *   - Per-voice ADSR envelope
 *   - Audio callback for Bsp_Audio (PCM mode)
 *   - Built-in sequencer for song playback
 *
 * All voices mix into a single 8-bit PCM stream sent to the
 * existing PWM audio output (TIMA0 + TIMG6).
 *
 * Works best when frequencies are in the buzzer's resonant range
 * (~800 Hz – 4 kHz).  Use Synth_NoteOn() from main-loop context
 * and Synth_Update() to drive the built-in sequencer ticks.
 */

/* ─── Configuration ──────────────────────────────────────────────────── */

#define SYNTH_NUM_VOICES  4
#define SYNTH_SAMPLE_RATE 16000  /* 16 kHz — sweet spot for quality vs CPU */

/* ─── Waveform types ─────────────────────────────────────────────────── */

typedef enum {
    SYNTH_WAVE_SINE,      /* pure sine — good for chords, quiet through buzzer */
    SYNTH_WAVE_SQUARE,    /* rich odd harmonics — cuts through best */
    SYNTH_WAVE_TRIANGLE,  /* softer odd harmonics — good for chords */
    SYNTH_WAVE_SAWTOOTH,  /* all harmonics — bright, buzzy */
    SYNTH_WAVE_NOISE,     /* white noise — percussion (snare, hi-hat) */
    SYNTH_WAVE_COUNT
} Synth_Waveform;

/* ─── Public API ─────────────────────────────────────────────────────── */

/**
 * @brief  Initialise the synth engine.
 *         Must be called once before any other synth function.
 */
void Synth_Init(void);

/**
 * @brief  Start the synth — registers the audio callback with bsp_audio.
 *         The buzzer will begin producing sound.
 */
void Synth_Start(void);

/**
 * @brief  Stop the synth and silence the buzzer.
 */
void Synth_Stop(void);

/**
 * @brief  Return 1 while the synth is actively rendering.
 */
uint8_t Synth_Is_Playing(void);

/* ─── Note control (for live / real-time use) ────────────────────────── */

/**
 * @brief  Start a note on a given voice.
 * @param  voice     Voice index (0 .. SYNTH_NUM_VOICES-1)
 * @param  freq_hz   Frequency in Hz (0 = rest/silence)
 * @param  velocity  Loudness 0-255
 * @param  waveform  Waveform type
 */
void Synth_NoteOn(uint8_t voice, uint16_t freq_hz, uint8_t velocity,
                  Synth_Waveform waveform);

/**
 * @brief  Stop the note on a given voice (enter release phase).
 */
void Synth_NoteOff(uint8_t voice);

/**
 * @brief  Immediately silence a voice (no release tail).
 */
void Synth_NoteOff_Immediate(uint8_t voice);

/**
 * @brief  Set master volume (0-255).
 */
void Synth_SetMasterVolume(uint8_t vol);

/* ─── Song / sequencer ───────────────────────────────────────────────── */

/**
 * Song event command codes (1 byte each, followed by payload).
 */
#define SYNTH_CMD_NOTE_ON     0x10
#define SYNTH_CMD_NOTE_OFF    0x11
#define SYNTH_CMD_WAIT_MS     0x20
#define SYNTH_CMD_TEMPO       0x30
#define SYNTH_CMD_VOLUME      0x40   /* master volume */
#define SYNTH_CMD_END         0xFF

/**
 * @brief  Start playing a song from the built-in sequencer.
 *         Call Synth_Update() periodically from the main loop.
 *
 * @param  song_data  Pointer to bytecode song data in flash/ROM.
 */
void Synth_PlaySong(const uint8_t* song_data);

/**
 * @brief  Stop the current song.
 */
void Synth_StopSong(void);

/**
 * @brief  Return 1 if the sequencer is actively playing a song.
 */
uint8_t Synth_IsSongPlaying(void);

/**
 * @brief  Call this periodically (every ~10-20 ms) from the main loop.
 *         Advances the song sequencer and handles note timing.
 */
void Synth_Update(void);

/* ─── Built-in demo songs ────────────────────────────────────────────── */

/** A demo song that demonstrates chords, melody, and percussion. */
extern const uint8_t synth_demo_song[];
