#pragma once

#include <stdint.h>

typedef struct Vector_t {
    void* p_data;
    uint32_t element_size;
    uint32_t total_size;
    uint32_t capacity;
} Vector;

Vector* Vector_Init(uint32_t element_size, uint32_t init_capacity);
void Vector_Deinit(Vector* obj);

void Vector_Push_Back(Vector* obj, void* element);

void* Vector_Get_At(const Vector* obj, uint32_t idx);

uint32_t Vector_Get_Size(const Vector* obj);
uint32_t Vector_Get_Capacity(const Vector* obj);