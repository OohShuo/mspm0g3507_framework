#include "vector.h"

#include <stdlib.h>
#include <string.h>

Vector* Vector_Init(uint32_t element_size, uint32_t init_capacity) {
    if (element_size == 0 || init_capacity == 0) return NULL;

    Vector* obj = malloc(sizeof(Vector));
    if (obj == NULL) return NULL;

    obj->p_data = malloc(element_size * init_capacity);
    if (obj->p_data == NULL) {
        free(obj);
        return NULL;
    }

    obj->element_size = element_size;
    obj->total_size = 0;
    obj->capacity = init_capacity;

    return obj;
}

void Vector_Deinit(Vector* obj) {
    if (obj == NULL) return;

    if (obj->p_data != NULL) {
        free(obj->p_data);
        obj->p_data = NULL;
    }
    obj->total_size = 0;
    obj->capacity = 0;
}

void Vector_Push_Back(Vector* obj, void* element) {
    if (obj == NULL || obj->p_data == NULL || element == NULL) return;

    if (obj->total_size >= obj->capacity) {
        uint32_t new_capacity = obj->capacity * 2;
        void* new_ptr = realloc(obj->p_data, new_capacity * obj->element_size);
        if (new_ptr == NULL) return;

        obj->p_data = new_ptr;
        obj->capacity = new_capacity;
    }

    uint8_t* target = (uint8_t*)obj->p_data + (obj->total_size * obj->element_size);
    memcpy(target, element, obj->element_size);
    obj->total_size++;
}

void* Vector_Get_At(const Vector* obj, uint32_t idx) {
    if (obj == NULL || obj->p_data == NULL || idx >= obj->total_size) { return NULL; }

    return (uint8_t*)obj->p_data + (idx * obj->element_size);
}

uint32_t Vector_Get_Size(const Vector* obj) { return (obj != NULL) ? obj->total_size : 0; }

uint32_t Vector_Get_Capacity(const Vector* obj) { return (obj != NULL) ? obj->capacity : 0; }