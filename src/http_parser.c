#include "http_parser.h"

uint32_t hash_fb(const char *s, int len) {
    uint32_t hash = 2166136261U;
    for (int i = 0; i < len; i++) {
        hash ^= (uint8_t)s[i];
        hash *= 16777619U;
    }
    return hash;
}

static inline uint8_t http_method(char *buffer) {
    if(buffer[0] == 'G')
        return GET;
    if(buffer[0] == 'P' && buffer[1] == 'O')
        return POST;
    return NONE;
}

void get_status(int fd) {
    const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    send(fd, response, 38, MSG_NOSIGNAL);
}

void routes_manager(struct Request request, int fd) {
    if(request.method == GET) get_status(fd);
    else if(request.method == POST) return;
    else return;
}
