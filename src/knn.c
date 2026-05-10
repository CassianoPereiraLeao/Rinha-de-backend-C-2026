#include "knn.h"
#include <float.h>

#define K 5
#define FRAUD_THRESHOLD 0.6f

KnnResult knn_classify(const float query[DIMENTIONS]) {
    float top_dist[K];
    uint8_t top_label[K];
    int found = 0;
    float worst = FLT_MAX;

    const int n = g_refs.count;

    for(int i = 0; i < n; i++) {
        float d = 0.0f;
        for(int dimention = 0; dimention < DIMENTIONS; dimention++) {
            float diff = query[dimention] - g_refs.dimentions[dimention][i];
            d += diff * diff;
        }

        if(found < K || d < worst) {
            if(found < K) {
                top_dist[found] = d;
                top_label[found] = g_refs.labels[i];
                found++;
                if(found == K) {
                    worst = 0.0f;
                    for(int j = 0; j < K; j++) 
                        if(top_dist[j] > worst) worst = top_dist[j];
                }
            } else {
                int worst_index = 0;
                for(int j = 1; j < K; j++) 
                    if(top_dist[j] > top_dist[worst_index]) worst_index = j;
                top_dist[worst_index] = d;
                top_label[worst_index] = g_refs.labels[i];
                worst = 0.0f;
                for(int j = 0; j < K; j++)
                    if(top_dist[j] > worst) worst = top_dist[j];
            }
        }
    }

    int fraud_count = 0;
    for(int j = 0; j < found; j++)
        if(top_label[j] == 1)
            fraud_count++;
    
    const float fraud_score = (float)fraud_count / (float)K;
    const bool approved = fraud_score < FRAUD_THRESHOLD;
    
    return (KnnResult) {
        .approved = approved,
        .fraud_score = fraud_score
    };
}
