#include "http_parser.h"

static const char RESP_0[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 35\r\n"
    "Connection: keep-alive\r\n\r\n"
    "{\"approved\":true,\"fraud_score\":0.0}";
 
static const char RESP_1[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 35\r\n"
    "Connection: keep-alive\r\n\r\n"
    "{\"approved\":true,\"fraud_score\":0.2}";
 
static const char RESP_2[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 35\r\n"
    "Connection: keep-alive\r\n\r\n"
    "{\"approved\":true,\"fraud_score\":0.4}";
 
static const char RESP_3[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 36\r\n"
    "Connection: keep-alive\r\n\r\n"
    "{\"approved\":false,\"fraud_score\":0.6}";
 
static const char RESP_4[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 36\r\n"
    "Connection: keep-alive\r\n\r\n"
    "{\"approved\":false,\"fraud_score\":0.8}";
 
static const char RESP_5[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 36\r\n"
    "Connection: keep-alive\r\n\r\n"
    "{\"approved\":false,\"fraud_score\":1.0}";
 
static const char RESP_400[] =
    "HTTP/1.1 400 Bad Request\r\n"
    "Connection: keep-alive\r\n"
    "Content-Length: 0\r\n\r\n";
 
static const char RESP_404[] =
    "HTTP/1.1 404 Not Found\r\n"
    "Connection: keep-alive\r\n"
    "Content-Length: 0\r\n\r\n";
 
static const char RESP_FALLBACK[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 35\r\n"
    "Connection: keep-alive\r\n\r\n"
    "{\"approved\":true,\"fraud_score\":0.0}";

static const struct { const char* data; size_t len; } BUCKETS[6] = {
    { RESP_0, sizeof(RESP_0) - 1 },
    { RESP_1, sizeof(RESP_1) - 1 },
    { RESP_2, sizeof(RESP_2) - 1 },
    { RESP_3, sizeof(RESP_3) - 1 },
    { RESP_4, sizeof(RESP_4) - 1 },
    { RESP_5, sizeof(RESP_5) - 1 },
};

uint32_t hash_fb(const char *s, int len) {
    uint32_t hash = 2166136261U;
    for (int i = 0; i < len; i++) {
        hash ^= (uint8_t)s[i];
        hash *= 16777619U;
    }
    return hash;
}

void set_response(Request *request, const char* data, size_t len) {
    request->write_ptr = data;
    request->write_len = len;
    request->write_pos = 0;
}

void routes_manager(Request *request, const char* body, size_t len) {
    const char* buffer = request->buffer_read;
    if(buffer[0] == 'G') {
        set_response(request, RESPONSE_READY, sizeof(RESPONSE_READY) - 1);
        return;
    }

    if(buffer[0] == 'P') {
        if(!g_refs_ready) {
            set_response(request, RESP_FALLBACK, sizeof(RESP_FALLBACK) - 1);
            return;
        }

        float vector[DIMENTIONS];
        if(!vectorize(body, len, vector)) {
            set_response(request, RESP_400, sizeof(RESP_400) - 1);
            return;
        }

        KnnResult result = knn_classify(vector);

        int bucket = (int)(result.fraud_score * 5.0f + 0.5f);
        if(bucket < 0) bucket = 0;
        if(bucket > 5) bucket = 5;

        set_response(request, BUCKETS[bucket].data, BUCKETS[bucket].len);
        return;
    }

    set_response(request, RESP_404, sizeof(RESP_404) - 1);
}
