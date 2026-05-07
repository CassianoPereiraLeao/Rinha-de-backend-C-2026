#pragma once

#include "common.h"

typedef struct {
    size_t arena_size;
    size_t current;
    char* arena_buffer;
} Arena;

Arena* init_arena(size_t arena_capacity);
void* arena_memory_alloc(Arena *arena, size_t size);
