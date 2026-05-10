#pragma once

#include "common.h"
#include "structs.h"
#include "vectorize.h"
#include "knn.h"
#include "refs.h"

void routes_manager(Request *request, const char* body, size_t len);