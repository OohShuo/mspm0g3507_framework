# VM Audio and Haptics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the SDL VM synthesized buzzer audio, visible vibration feedback, and optional controller rumble.

**Architecture:** A pure sample renderer consumes existing `Music` data. SDL-specific wrappers own the audio device and haptic controller; worker threads communicate vibration intensity through SDL atomics.

**Tech Stack:** C99, SDL2 audio/game-controller APIs, existing Buzzer and Vib_motor HAL interfaces.

## Global Constraints

- No WAV assets or new dependencies.
- Audio/haptic initialization failures must not prevent VM startup.
- No allocation or blocking calls inside the SDL audio callback.
- SDL controller calls stay on the main thread.

---

### Task 1: Pure audio synthesizer and SDL buzzer backend

**Files:** Create `src/vm/audio_synth_vm.c/.h`, `tests/vm_audio_synth_test.c`; modify `src/vm/hal/buzzer_vm.c`, `src/vm/CMakeLists.txt`.

- [ ] Write a failing host test that plays a short `Music`, checks non-zero gated samples, volume scaling, and eventual silence.
- [ ] Implement `Vm_Audio_Synth_Init`, `Play`, `Stop`, `Set_Volume`, `Is_Active`, and `Render` without SDL calls.
- [ ] Replace the buzzer no-op with an SDL callback protected by `SDL_LockAudioDevice`.
- [ ] Compile `src/hal/buzzer/buzzer_def.c` into the VM target and run the host test.

### Task 2: VM vibration patterns and haptics bridge

**Files:** Create `src/vm/haptics_vm.c/.h`, `tests/vm_haptics_test.c`; modify `src/vm/hal/vib_motor_vm.c`, `src/vm/hal/hal_vm.c`.

- [ ] Write a failing test for atomic strength set/get/clear.
- [ ] Implement the haptics bridge, controller hot-plug, rumble updates, and graceful no-controller behavior.
- [ ] Implement VM effect patterns and call `Vm_Haptics_Set_Strength` as steps start/stop.
- [ ] Initialize/update Vib_motor from VM HAL and run focused tests.

### Task 3: Visual feedback and lifecycle integration

**Files:** Modify `src/vm/display_vm.c`, `src/vm/main_vm.c`; create `tests/vm_feedback_integration_test.py`.

- [ ] Write failing source-contract checks for event forwarding, haptics update/deinit, and overlay rendering.
- [ ] Draw a proportional orange border and bottom strength bar after the LCD texture.
- [ ] Route controller events and lifecycle calls through the SDL main loop.
- [ ] Run focused tests, VM build, dummy-audio startup smoke test, diff check, and commit.
