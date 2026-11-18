#pragma once
#include "diagnostics.h"
bool diag_export_start(void);
/* Owner-only, zero-wait, bounded one-record queue. Never writes serial in UI. */
bool diag_export_submit(const diag_record_t *record);
