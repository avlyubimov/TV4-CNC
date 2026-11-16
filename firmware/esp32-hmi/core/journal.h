#pragma once
#include <stdbool.h>
#include <stdint.h>
#define JOURNAL_CAPACITY 32u
typedef enum { F_NONE, F_EMERGENCY, F_PROTOCOL, F_QUEUE, F_TOUCH, F_DISPLAY, F_INVALID } fault_t;
typedef enum { J_BOOT, J_STEP, J_STOP, J_DISCONNECT, J_CONNECT, J_FAULT, J_ACK, J_INCREMENT, J_REJECTED, J_TIMEOUT, J_RESTART, J_TOOL_SAVE, J_TOOL_SELECT, J_RPM, J_OVERRIDE, J_FRAME, J_ZERO_X, J_ZERO_Z, J_ZERO_ALL } journal_kind_t;
typedef struct { uint32_t number, ms; journal_kind_t kind; fault_t fault; } journal_entry_t;
typedef struct {
    journal_entry_t entries[JOURNAL_CAPACITY];
    unsigned head,count;
    uint32_t total, overwritten;
} journal_t;
void journal_init(journal_t *journal);
void journal_push(journal_t *journal,uint32_t ms,journal_kind_t kind,fault_t fault);
/* Index 0 is newest. Returns a copy, never exposes mutable ring storage. */
bool journal_get(const journal_t *journal,unsigned index,journal_entry_t *entry);
