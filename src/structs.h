#pragma once

#include "common.h"

#define BYTES_READ_CAPACITY (1024 * 16)

// MACRO PARA RESPOSTA MAIS ÁGIL
#define RESPONSE_READY "HTTP/1.1 204 No Content\r\n" \
    "Connection: keep-alive\r\n" \
    "Content-Length: 0\r\n\r\n" \

struct Response {
    bool approved;
    float fraud_score;
};

typedef struct {
    uint32_t max_amount;
    uint8_t max_installments;
    uint8_t amount_vs_avg_ratio;
    uint16_t max_minutes;
    uint16_t max_km;
    uint8_t max_tx_count_24h;
    uint32_t max_merchant_avg_amount;
} Normalize;

typedef struct Request {
    int fd;
    char buffer_read[BYTES_READ_CAPACITY];
    size_t read_len;
    size_t write_pos;
    size_t write_len;
    const char* write_ptr;
    uint32_t epoll_events;
    struct Request* next;
} Request;
