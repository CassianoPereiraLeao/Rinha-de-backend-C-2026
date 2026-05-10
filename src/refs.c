#include "refs.h"

Refs g_refs;
volatile int g_refs_ready = 0;

int refs_load(const char* refs_path, const char* labels_path) {
    FILE* f = fopen(refs_path, "rb");
    if(!f) {
        fprintf(stderr, "refs_load: não abriu %s\n", refs_path);
        return -1;
    }

    const size_t expected = sizeof(float) * DIMENTIONS * REFS_COUNT;
    const size_t got = fread(g_refs.dimentions, 1, expected, f);
    fclose(f);
    if(got != expected) {
        fprintf(stderr, "refs_load: leu %zu bytes, esperava %zu\n", got, expected);
        return -1;
    }

    f = fopen(labels_path, "rb");
    if(!f) {
        fprintf(stderr, "refs_load: não abriu %s\n", labels_path);
        return -1;
    }

    const size_t got2 = fread(g_refs.labels, 1, REFS_COUNT, f);
    if(got2 != expected) {
        fprintf(stderr, "refs_load: labels leu %zu, esperava %d\n", got2, REFS_COUNT);
        return -1;
    }

    g_refs.count = REFS_COUNT;
    g_refs_ready = 1;
    return 0;
}
