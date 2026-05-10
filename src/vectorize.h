#pragma once

#include "common.h"
#include "refs.h"
#include <math.h>

float mcc_risk_get(const char* mcc, int mcc_len);
bool vectorize(const char* body, size_t len, float out[DIMENTIONS]);
