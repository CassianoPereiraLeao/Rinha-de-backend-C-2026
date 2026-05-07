#include "http_parser.h"

typedef enum {
    NONE,
    GET,
    POST
} HttpState;

struct request {
    HttpState method;
    char* path;
};

uint32_t hash_fb(const char *s, int len) {
    uint32_t hash = 2166136261U;
    for (int i = 0; i < len; i++) {
        hash ^= (uint8_t)s[i];
        hash *= 16777619U;
    }
    return hash;
}

HttpState http_method(char *buffer) {
    if(buffer[0] == 'G')
        return GET;
    if(buffer[0] == 'P' && buffer[1] == 'O')
        return POST;
    return NONE;
}
