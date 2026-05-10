#pragma once

#include "common.h"

#define REFS_COUNT 100000
#define DIMENTIONS 14

typedef struct {
    float dimentions[DIMENTIONS][REFS_COUNT];
    uint8_t labels[REFS_COUNT];
    int count;
} Refs;

extern Refs g_refs;
extern volatile int g_refs_ready;

int refs_load(const char* refs_path, const char* labels_path);
