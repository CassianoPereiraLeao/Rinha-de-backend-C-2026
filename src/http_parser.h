#pragma once

#include "common.h"
#include "structs.h"

#define HASH_TRANSACTION 0x75095723
#define HASH_AMOUNT 0xF785CE49
#define HASH_INSTALLMENTS 0x79AF4F25
#define HASH_REQUESTED_AT 0xC603D3C1
#define HASH_ID 0x37386AE0
#define HASH_CUSTOMER 0xA4BDAA19
#define HASH_AVG_AMOUNT 0x886D86B0
#define HASH_TX_COUNT_24H 0x56B5C0F4
#define HASH_KNOWN_MERCHANTS 0xEE66A60E
#define HASH_MERCHANT 0x1A2D0ED5
#define HASH_MCC 0xBCA8C8C6
#define HASH_AVG_AMOUNT 0x886D86B0
#define HASH_TERMINAL 0x99F40AB1
#define HASH_IS_ONLINE 0x5467E2E5
#define HASH_CARD_PRESENT 0xDAE5E50B
#define HASH_KM_FROM_HOME 0xE9941EA4
#define HASH_LAST_TRANSACTION 0xE019966E
#define HASH_TIMESTAMP 0xB283D523
#define HASH_KM_FROM_CURRENT 0x4DAAE720

typedef enum {
    NONE,
    GET,
    POST
} HttpState;

static inline uint8_t http_method(char *buffer);
void routes_manager(struct Request request, int fd);
