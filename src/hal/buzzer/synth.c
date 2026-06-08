#include "synth.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "bsp_audio.h"
#include "bsp_time.h"

/* ─── Tuning / constants ────────────────────────────────────────────── */

/** Maximum value of the phase accumulator (Q16.16 fixed-point). */
#define PHASE_MAX   (1UL << 16)

/** Size of the pre-computed sine wavetable.  Must be a power of two. */
#define WAVE_TABLE_BITS  8
#define WAVE_TABLE_SIZE  (1 << WAVE_TABLE_BITS)
#define WAVE_TABLE_MASK  (WAVE_TABLE_SIZE - 1)

/** Envelope time constants (sample counts at 16 kHz). */
#define ENV_ATTACK_SAMPLES  200    /* ~12.5 ms attack  */
#define ENV_DECAY_SAMPLES   400    /* ~25 ms decay     */
#define ENV_RELEASE_SAMPLES 600    /* ~37.5 ms release */
#define ENV_SUSTAIN_LEVEL   1800   /* 0-4095 sustain level */

/** Double-buffer size for PCM audio. */
#define AUDIO_BUF_SIZE  512

/* ─── Envelope state machine ────────────────────────────────────────── */

typedef enum {
    ENV_IDLE,
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE,
} EnvState;

/* ─── Per-voice state ───────────────────────────────────────────────── */

typedef struct {
    uint8_t  active;        /* nonzero = producing sound */
    uint8_t  waveform;      /* Synth_Waveform */
    uint8_t  velocity;      /* 0-255 */

    uint32_t phase_acc;     /* Q16.16 phase accumulator */
    uint32_t phase_step;    /* Q16.16 increment per sample */

    /* ADSR envelope */
    EnvState env_state;
    uint32_t env_counter;   /* sample count in current envelope phase */
    uint16_t env_level;     /* Q4.12 current amplitude (0-4095) */
} Voice;

/* ─── Global state ──────────────────────────────────────────────────── */

static Voice      voices[SYNTH_NUM_VOICES];
static uint8_t    master_vol = 200;
static uint8_t    playing    = 0;

/* Double-buffer for PCM audio */
static uint8_t    audio_buffer[AUDIO_BUF_SIZE];

/* Pre-computed sine wavetable (Q4.12, signed 0-4095 centred at 2048) */
static uint16_t   sine_table[WAVE_TABLE_SIZE];

/* ─── Song sequencer state ──────────────────────────────────────────── */

static const uint8_t* song_ptr    = NULL;
static uint8_t        song_playing = 0;
static uint32_t       song_tick_ms = 50;   /* default tick = 50 ms */
static uint32_t       song_wait_start = 0;
static uint32_t       song_wait_ms    = 0;
static uint32_t       song_tempo_base = 50;

/* ─── Helpers ────────────────────────────────────────────────────────── */

/** Phase step for a given frequency at the current sample rate. */
static inline uint32_t freq_to_phase_step(uint16_t freq_hz) {
    /* phase_step = (freq * PHASE_MAX) / SAMPLE_RATE */
    uint32_t step = ((uint32_t)freq_hz << 16) / SYNTH_SAMPLE_RATE;
    return step;
}

/** Lookup a wavetable sample for a given waveform and phase. */
static inline int16_t wave_lookup(uint8_t waveform, uint32_t phase) {
    uint16_t idx = (phase >> (16 - WAVE_TABLE_BITS)) & WAVE_TABLE_MASK;

    switch (waveform) {

    case SYNTH_WAVE_SINE: {
        int16_t v = (int16_t)sine_table[idx] - 2048;  /* signed, -2048..2047 */
        return v;
    }

    case SYNTH_WAVE_SQUARE: {
        /* Square wave: +2047 for first half, -2048 for second half */
        return (phase < PHASE_MAX / 2) ? 2047 : -2048;
    }

    case SYNTH_WAVE_TRIANGLE: {
        /* Triangle: up-ramp for first half, down-ramp for second */
        uint32_t half = PHASE_MAX / 2;
        if (phase < half) {
            return (int16_t)((int32_t)phase * 4095 / half - 2048);
        } else {
            return (int16_t)((int32_t)(PHASE_MAX - phase) * 4095 / half - 2048);
        }
    }

    case SYNTH_WAVE_SAWTOOTH: {
        /* Sawtooth: linear ramp up */
        return (int16_t)((int32_t)phase * 4095 / PHASE_MAX - 2048);
    }

    case SYNTH_WAVE_NOISE: {
        /* Pseudo-random noise — use a simple LFSR */
        static uint16_t lfsr = 0xACE1;
        lfsr = (lfsr >> 1) ^ (-(lfsr & 1) & 0xB400);
        return ((int16_t)lfsr) >> 2;  /* ~ -2048..2047 */
    }

    default:
        return 0;
    }
}

/* ─── Envelope processing ────────────────────────────────────────────── */

/** Process one sample of the envelope for a voice. */
static inline void envelope_tick(Voice* v) {
    switch (v->env_state) {

    case ENV_IDLE:
        v->env_level = 0;
        break;

    case ENV_ATTACK:
        v->env_counter++;
        if (v->env_counter >= ENV_ATTACK_SAMPLES) {
            v->env_level = 4095;
            v->env_state = ENV_DECAY;
            v->env_counter = 0;
        } else {
            /* Linear attack ramp */
            v->env_level = (uint16_t)((uint32_t)v->env_counter * 4095
                                      / ENV_ATTACK_SAMPLES);
        }
        break;

    case ENV_DECAY:
        v->env_counter++;
        if (v->env_counter >= ENV_DECAY_SAMPLES) {
            v->env_level = ENV_SUSTAIN_LEVEL;
            v->env_state = ENV_SUSTAIN;
            v->env_counter = 0;
        } else {
            v->env_level = (uint16_t)(4095 -
                ((uint32_t)(4095 - ENV_SUSTAIN_LEVEL) * v->env_counter
                 / ENV_DECAY_SAMPLES));
        }
        break;

    case ENV_SUSTAIN:
        /* Held at sustain level until NoteOff */
        break;

    case ENV_RELEASE:
        v->env_counter++;
        if (v->env_counter >= ENV_RELEASE_SAMPLES) {
            v->env_level = 0;
            v->env_state = ENV_IDLE;
            v->active = 0;
        } else {
            v->env_level = (uint16_t)(ENV_SUSTAIN_LEVEL -
                ((uint32_t)ENV_SUSTAIN_LEVEL * v->env_counter
                 / ENV_RELEASE_SAMPLES));
        }
        break;
    }
}

/* ─── Audio callback (called from TIMG6 ISR) ─────────────────────────── */

static void Synth_Generate(uint8_t* buffer, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        int32_t mixed = 0;

        for (uint32_t v = 0; v < SYNTH_NUM_VOICES; v++) {
            Voice* voice = &voices[v];
            if (!voice->active && voice->env_state == ENV_IDLE)
                continue;

            /* Advance envelope */
            envelope_tick(voice);

            /* Advance phase accumulator */
            voice->phase_acc += voice->phase_step;
            voice->phase_acc &= (PHASE_MAX - 1);  /* wrap at 2^16 */

            /* Lookup waveform */
            int16_t sample = wave_lookup(voice->waveform, voice->phase_acc);

            /* Apply envelope + velocity */
            int32_t amp = (int32_t)sample * voice->env_level * voice->velocity
                          / (4095 * 255);

            mixed += amp;
        }

        /* Apply master volume and soft-clip */
        mixed = mixed * (int32_t)master_vol / 255;

        /* Soft-clip: tanh-like shaping for a warmer sound */
        if (mixed > 15000)      mixed = 15000;
        else if (mixed < -15000) mixed = -15000;

        /* Convert to 8-bit unsigned PCM (0 = silence, 128 = midpoint) */
        int32_t unsigned_val = (mixed * 255) / 15000 + 128;
        if (unsigned_val < 0)   unsigned_val = 0;
        if (unsigned_val > 255) unsigned_val = 255;

        buffer[i] = (uint8_t)unsigned_val;
    }
}

/* ─── Public API ─────────────────────────────────────────────────────── */

void Synth_Init(void) {
    /* Pre-compute sine wavetable (Q4.12, centred at 2048) */
    for (int i = 0; i < WAVE_TABLE_SIZE; i++) {
        double angle = 2.0 * 3.14159265358979 * i / WAVE_TABLE_SIZE;
        double val = sin(angle);
        sine_table[i] = (uint16_t)((val * 0.5 + 0.5) * 4095.0);
    }

    /* Clear all voices */
    memset(voices, 0, sizeof(voices));
}

void Synth_Start(void) {
    if (playing) return;

    /* Reset voice envelopes */
    for (int i = 0; i < SYNTH_NUM_VOICES; i++) {
        voices[i].env_state = ENV_IDLE;
        voices[i].env_level = 0;
        voices[i].active = 0;
    }

    playing = 1;

    /* Fire up the PCM audio system with our callback */
    Bsp_Audio_Start(Synth_Generate, audio_buffer, AUDIO_BUF_SIZE,
                    SYNTH_SAMPLE_RATE);
}

void Synth_Stop(void) {
    playing = 0;
    song_playing = 0;
    Bsp_Audio_Stop();
}

uint8_t Synth_Is_Playing(void) {
    return playing;
}

/* ─── Note control ──────────────────────────────────────────────────── */

void Synth_NoteOn(uint8_t voice, uint16_t freq_hz, uint8_t velocity,
                  Synth_Waveform waveform) {
    if (voice >= SYNTH_NUM_VOICES) return;

    Voice* v = &voices[voice];
    v->waveform   = (uint8_t)waveform;
    v->velocity   = velocity;
    v->phase_step = freq_to_phase_step(freq_hz);
    v->phase_acc  = 0;

    /* Start attack (unless frequency is 0 = rest) */
    if (freq_hz > 0) {
        v->active = 1;
        v->env_state  = ENV_ATTACK;
        v->env_counter = 0;
        v->env_level   = 0;
    } else {
        v->active = 0;
        v->env_state = ENV_IDLE;
    }
}

void Synth_NoteOff(uint8_t voice) {
    if (voice >= SYNTH_NUM_VOICES) return;
    Voice* v = &voices[voice];
    if (v->env_state != ENV_IDLE && v->env_state != ENV_RELEASE) {
        v->env_state  = ENV_RELEASE;
        v->env_counter = 0;
    }
}

void Synth_NoteOff_Immediate(uint8_t voice) {
    if (voice >= SYNTH_NUM_VOICES) return;
    Voice* v = &voices[voice];
    v->active    = 0;
    v->env_state = ENV_IDLE;
    v->env_level = 0;
}

void Synth_SetMasterVolume(uint8_t vol) {
    master_vol = vol;
}

/* ─── Sequencer ─────────────────────────────────────────────────────── */

void Synth_PlaySong(const uint8_t* song_data) {
    if (song_data == NULL) return;

    song_ptr    = song_data;
    song_playing = 1;
    song_wait_ms = 0;
    song_wait_start = 0;
    song_tick_ms = song_tempo_base;

    /* Make sure the synth is running */
    if (!playing) Synth_Start();
}

void Synth_StopSong(void) {
    song_playing = 0;
    song_ptr = NULL;
    /* Release all held notes */
    for (int i = 0; i < SYNTH_NUM_VOICES; i++) {
        Synth_NoteOff(i);
    }
}

uint8_t Synth_IsSongPlaying(void) {
    return song_playing;
}

void Synth_Update(void) {
    if (!song_playing || song_ptr == NULL) return;

    uint32_t now = Bsp_Get_Tick_Ms();

    /* If waiting, check if time has elapsed */
    if (song_wait_ms > 0) {
        if (song_wait_start == 0) {
            song_wait_start = now;
        }
        if (now - song_wait_start >= song_wait_ms) {
            song_wait_ms = 0;
            song_wait_start = 0;
            /* Resume processing from saved position */
        } else {
            return;  /* Still waiting */
        }
    }

    /* Process events from the song bytecode.
     *
     * Bytecode format:
     *   <cmd:1> <payload:N>
     *
     * Commands:
     *   0x10 NOTE_ON  : voice(1) freq_hi(1) freq_lo(1) vel(1) wave(1)
     *   0x11 NOTE_OFF : voice(1)
     *   0x20 WAIT_MS  : dur_hi(1) dur_lo(1)  — wait N ms
     *   0x30 TEMPO    : tick_ms(1)            — set tick base
     *   0x40 VOLUME   : vol(1)                — master volume
     *   0xFF END
     */
    const uint8_t* p = song_ptr;

    while (1) {
        if (p == NULL) { song_playing = 0; return; }

        uint8_t cmd = *p++;
        switch (cmd) {

        case SYNTH_CMD_NOTE_ON: {
            uint8_t v  = *p++;
            uint16_t freq;
            freq  = (uint16_t)(*p++) << 8;
            freq |= *p++;
            uint8_t vel    = *p++;
            uint8_t wave   = *p++;
            Synth_NoteOn(v, freq, vel, (Synth_Waveform)wave);
            break;
        }

        case SYNTH_CMD_NOTE_OFF: {
            uint8_t v = *p++;
            Synth_NoteOff(v);
            break;
        }

        case SYNTH_CMD_WAIT_MS: {
            uint16_t dur;
            dur  = (uint16_t)(*p++) << 8;
            dur |= *p++;
            /* Save position and start the wait timer */
            song_ptr = p;
            song_wait_ms = dur;
            song_wait_start = now;
            return;  /* Resume next Update() call */
        }

        case SYNTH_CMD_TEMPO: {
            song_tick_ms = *p++;
            break;
        }

        case SYNTH_CMD_VOLUME: {
            Synth_SetMasterVolume(*p++);
            break;
        }

        case SYNTH_CMD_END:
        default:
            song_playing = 0;
            song_ptr = NULL;
            return;
        }
    }
}
