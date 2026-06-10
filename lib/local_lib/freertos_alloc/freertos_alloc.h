#pragma once

#include <stddef.h>

void* pvPortMalloc(size_t xSize);  // NOLINT (readability-identifier-naming)
void vPortFree(void* pv);          // NOLINT (readability-identifier-naming)
