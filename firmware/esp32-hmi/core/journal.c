#include "journal.h"
#include <string.h>
void journal_init(journal_t *j) { memset(j,0,sizeof(*j)); }
void journal_push(journal_t *j,uint32_t ms,journal_kind_t kind,fault_t fault) {
    j->entries[j->head]=(journal_entry_t){.number=++j->total,.ms=ms,.kind=kind,.fault=fault};
    j->head=(j->head+1)%JOURNAL_CAPACITY;
    if(j->count<JOURNAL_CAPACITY) j->count++; else j->overwritten++;
}
bool journal_get(const journal_t *j,unsigned index,journal_entry_t *entry) {
    if(!j || !entry || index>=j->count) return false;
    *entry=j->entries[(j->head+JOURNAL_CAPACITY-1-index)%JOURNAL_CAPACITY]; return true;
}
