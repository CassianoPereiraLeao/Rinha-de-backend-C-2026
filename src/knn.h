#pragma once

#include "common.h"
#include "refs.h"

typedef struct {
    bool approved;
    float fraud_score;
} KnnResult;

KnnResult knn_classify(const float query[DIMENTIONS]);
