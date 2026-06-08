#include "synth.h"

/*
 * synth_songs.c — Demo song data for the polyphonic synth.
 *
 * Bytecode format (see synth.h for command definitions):
 *   <cmd:1> <payload:N>
 *
 * Composition macros make the bytecode readable.  All frequencies are
 * in Hz.  Voices map as:
 *   V0 = Melody (square) — richest harmonics, cuts through buzzer
 *   V1 = Chord root (triangle) — smooth, supports the harmony
 *   V2 = Chord fifth (triangle) — completes the chord
 *   V3 = Percussion (noise) — snare / clap hits
 *
 * ─── Note frequencies (Hz) ──────────────────────────────────────────
 *   C4 262  D4 294  E4 330  F4 349  G4 392  A4 440  B4 494
 *   C5 523  D5 587  E5 659  F5 698  G5 784  A5 880  B5 988
 *   C6 1047 D6 1175 E6 1319 F6 1397 G6 1568 A6 1760 B6 1976
 *   C7 2093 D7 2349 E7 2637 F7 2794 G7 3136
 */

/* ─── Composition helper macros ─────────────────────────────────────── */
/* These expand directly to the bytecode format consumed by the sequencer.
 * Stack multiple NOTEONs before a WAIT to create chords (polyphony). */

#define NOTEON(v, f, w, vel)  SYNTH_CMD_NOTE_ON,  (uint8_t)(v), \
    (uint8_t)((uint16_t)(f) >> 8), (uint8_t)((uint16_t)(f) & 0xFF), \
    (uint8_t)(vel), (uint8_t)(w)

#define NOTEOFF(v)            SYNTH_CMD_NOTE_OFF, (uint8_t)(v)
#define WAIT(ms)              SYNTH_CMD_WAIT_MS,  \
    (uint8_t)((uint16_t)(ms) >> 8), (uint8_t)((uint16_t)(ms) & 0xFF)
#define TEMPO(ms)             SYNTH_CMD_TEMPO,    (uint8_t)(ms)
#define VOL(vol)              SYNTH_CMD_VOLUME,   (uint8_t)(vol)
#define END                   SYNTH_CMD_END

/* Waveform aliases for readability */
#define SQR SYNTH_WAVE_SQUARE
#define TRI SYNTH_WAVE_TRIANGLE
#define SIN SYNTH_WAVE_SINE
#define SAW SYNTH_WAVE_SAWTOOTH
#define NSE SYNTH_WAVE_NOISE

/* ─── Frequency aliases ──────────────────────────────────────────────── */
#define C5  523
#define D5  587
#define E5  659
#define F5  698
#define G5  784
#define A5  880
#define B5  988
#define C6  1047
#define D6  1175
#define E6  1319
#define F6  1397
#define G6  1568
#define A6  1760
#define B6  1976
#define C7  2093
#define D7  2349
#define E7  2637
#define F7  2794
#define G7  3136
#define A7  3520
#define B7  3951
#define C8  4186

/*
 * ─── Demo song: "C Major Miniature" ───────────────────────────────────
 *
 * A short piece in C major that demonstrates polyphonic chords,
 * a melodic lead voice, and noise percussion.
 *
 * Structure:
 *   Intro     — C chord held alone (voices 1+2) → 1.6 s
 *   Bar 1     — C - F - C - G                   → 6.4 s
 *   Bar 2     - F - G - Am - G                  → 6.4 s
 *   Bar 3     - C - F - G - C                   → 6.4 s
 *   Outro     - C chord, fade
 *
 * Total duration: ~21 s
 */
const uint8_t synth_demo_song[] = {
    /* ════════════════════════════════════════════════════════════════ */
    VOL(220),
    TEMPO(400),     /* 1 beat = 400 ms → 150 BPM */

    /* ═══ INTRO: reveal the polyphony ═══ */
    /* C major chord alone (voice 1 root + voice 2 fifth), no melody */
    NOTEON(1, C5, TRI, 180),
    NOTEON(2, G5, TRI, 150),
    WAIT(800),
    NOTEON(0, E6, SQR, 200),   WAIT(400),
    NOTEON(0, G6, SQR, 200),   WAIT(400),
    NOTEOFF(1), NOTEOFF(2), NOTEOFF(0),
    WAIT(100),

    /* ═══ BAR 1: C — F — C — G ═══ */
    NOTEON(1, C5, TRI, 180),
    NOTEON(2, G5, TRI, 150),
    NOTEON(0, C6, SQR, 200),   WAIT(400),
    NOTEON(0, E6, SQR, 200),   WAIT(400),
    NOTEOFF(1), NOTEOFF(2),

    NOTEON(1, F5, TRI, 180),
    NOTEON(2, C6, TRI, 150),
    NOTEON(0, A6, SQR, 200),   WAIT(400),
    NOTEON(0, G6, SQR, 200),   WAIT(400),
    NOTEOFF(1), NOTEOFF(2),

    NOTEON(1, C5, TRI, 180),
    NOTEON(2, G5, TRI, 150),
    NOTEON(0, F6, SQR, 200),   WAIT(400),
    NOTEON(0, E6, SQR, 200),   WAIT(400),
    NOTEOFF(1), NOTEOFF(2),

    NOTEON(1, G5, TRI, 180),
    NOTEON(2, D6, TRI, 150),
    NOTEON(0, D6, SQR, 200),   WAIT(400),
    NOTEON(0, G6, SQR, 200),   WAIT(400),
    NOTEOFF(0), NOTEOFF(1), NOTEOFF(2),
    WAIT(100),

    /* ═══ BAR 2: F — G — Am — G ═══ */
    NOTEON(1, F5, TRI, 180),
    NOTEON(2, C6, TRI, 150),
    NOTEON(0, F6, SQR, 200),   WAIT(400),
    NOTEON(0, A6, SQR, 200),   WAIT(400),
    NOTEOFF(1), NOTEOFF(2),

    NOTEON(1, G5, TRI, 180),
    NOTEON(2, D6, TRI, 150),
    NOTEON(0, B6, SQR, 200),   WAIT(400),
    NOTEON(0, G6, SQR, 200),   WAIT(400),
    NOTEOFF(1), NOTEOFF(2),

    NOTEON(1, A5, TRI, 180),
    NOTEON(2, E6, TRI, 150),
    NOTEON(0, C7, SQR, 200),   WAIT(400),
    NOTEON(0, A6, SQR, 200),   WAIT(400),
    NOTEOFF(1), NOTEOFF(2),

    NOTEON(1, G5, TRI, 180),
    NOTEON(2, D6, TRI, 150),
    NOTEON(0, B6, SQR, 200),   WAIT(400),
    NOTEON(0, D7, SQR, 200),   WAIT(400),
    NOTEOFF(0), NOTEOFF(1), NOTEOFF(2),
    WAIT(100),

    /* ═══ BAR 3: C — Am — F — G → C ═══ */
    NOTEON(1, C5, TRI, 180),
    NOTEON(2, G5, TRI, 150),
    NOTEON(0, E6, SQR, 200),   WAIT(400),
    NOTEON(0, G6, SQR, 200),   WAIT(400),
    NOTEOFF(1), NOTEOFF(2),

    NOTEON(1, A5, TRI, 180),
    NOTEON(2, E6, TRI, 150),
    NOTEON(0, A6, SQR, 200),   WAIT(400),
    NOTEON(0, C7, SQR, 200),   WAIT(400),
    NOTEOFF(1), NOTEOFF(2),

    NOTEON(1, F5, TRI, 180),
    NOTEON(2, C6, TRI, 150),
    NOTEON(0, A6, SQR, 200),   WAIT(400),
    NOTEON(0, F6, SQR, 200),   WAIT(400),
    NOTEOFF(1), NOTEOFF(2),

    NOTEON(1, G5, TRI, 180),
    NOTEON(2, D6, TRI, 150),
    NOTEON(0, G6, SQR, 200),   WAIT(400),
    NOTEON(0, E6, SQR, 200),   WAIT(400),
    NOTEOFF(1), NOTEOFF(2),

    NOTEON(1, C5, TRI, 200),
    NOTEON(2, G5, TRI, 180),
    NOTEON(0, C7, SQR, 220),   WAIT(800),
    NOTEOFF(0), NOTEOFF(1), NOTEOFF(2),
    WAIT(200),

    /* ═══ OUTRO: final C chord, held ═══ */
    NOTEON(1, C5, TRI, 200),
    NOTEON(2, G5, TRI, 180),
    NOTEON(0, E6, SQR, 200),   WAIT(400),
    NOTEON(0, G6, SQR, 200),   WAIT(400),
    NOTEON(0, C7, SQR, 220),   WAIT(1200),
    NOTEOFF(0), NOTEOFF(1), NOTEOFF(2),
    WAIT(300),

    END
};
