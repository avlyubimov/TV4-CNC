#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "tv4_namespace.h"
#include "cJSON.h"
/* Private bounded parser. NULL on encoding/grammar/duplicate/size/OOM error.
 * Caller owns the returned tree. No hooks installed by production code. */
cJSON *tv4_ota_json_parse(const uint8_t *raw,size_t length,size_t maximum);
