#if !FRAMEWORK_USE_LVGL

// NOLINTBEGIN (readability-identifier-naming)

void lv_freertos_task_switch_in(const char* name) { (void)name; }

void lv_freertos_task_switch_out(void) {}

// NOLINTEND (readability-identifier-naming)

#endif
