#pragma once

#include "common.h"
#include "arena.h"

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

struct Request {
    uint8_t method;
    Arena *arena;
};
