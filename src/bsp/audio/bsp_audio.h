#pragma once

#include <stdint.h>

/**
 * Audio playback through passive buzzer using PWM duty-cycle modulation.
 *
 * Principle: PWM carrier at ~50kHz is inaudible. The buzzer's mechanical
 * structure acts as a low-pass filter, extracting the audio envelope
 * from the rapidly varying duty cycle.
 *
 * Hardware: TIMA0 (buzzer PWM) as carrier, TIMG6 as sample clock interrupt.
 */

/**
 * @brief Audio sample callback — called from ISR at sample_rate Hz.
 *        Fill buffer with audio samples (8-bit unsigned, 0=silence, 128=mid, 255=max).
 *        Must be fast! No blocking, no printf, no FreeRTOS API.
 *
 * @param buffer  Pointer to sample buffer to fill
 * @param length  Number of samples to write
 */
typedef void (*Bsp_Audio_Callback)(uint8_t* buffer, uint32_t length);

/**
 * @brief  Initialize audio subsystem (no-op until Start is called).
 */
void Bsp_Audio_Init(void);

/**
 * @brief  Start audio playback with a streaming callback.
 *         Reconfigures the buzzer PWM timer to audio mode.
 *
 * @param callback    ISR callback to provide audio samples (streaming mode)
 * @param buffer      Double-buffer for DMA/callback use
 * @param buf_len     Buffer length in samples
 * @param sample_rate Sample rate in Hz (e.g. 8000)
 */
void Bsp_Audio_Start(Bsp_Audio_Callback callback, uint8_t* buffer, uint32_t buf_len, uint32_t sample_rate);

/**
 * @brief  Start audio playback from a pre-filled ROM buffer (one-shot).
 *
 * @param data        Pointer to 8-bit unsigned audio data in ROM/flash
 * @param length      Number of samples
 * @param sample_rate Sample rate in Hz (e.g. 8000)
 */
void Bsp_Audio_Start_Buffer(const uint8_t* data, uint32_t length, uint32_t sample_rate);

/**
 * @brief  Stop audio playback and restore PWM to music-note mode.
 */
void Bsp_Audio_Stop(void);

/**
 * @brief  Returns 1 if audio is currently playing.
 */
uint8_t Bsp_Audio_Is_Playing(void);
