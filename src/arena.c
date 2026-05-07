#include "arena.h"

Arena* init_arena(size_t arena_capacity) {
    Arena* arena = (Arena*)malloc(sizeof(Arena));
    arena->arena_buffer = (char*)malloc(arena_capacity);
    arena->arena_size = arena_capacity;
    arena->current = 0;
    return arena;
}

void* arena_memory_alloc(Arena *arena, size_t size) {
    size_t aligned_size = (size + 7)& ~7;
    if(arena->current + aligned_size <= arena->arena_size) {
        void* pointer = &arena->arena_buffer[arena->current];
        arena->current = aligned_size;
        return pointer;
    }

    return NULL;
}

void arena_memory_reset(Arena *arena) { arena->current = 0; }
