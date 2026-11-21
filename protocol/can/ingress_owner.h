#ifndef TV4_INGRESS_OWNER_H
#define TV4_INGRESS_OWNER_H
#include "frame_ingress.h"
#include "update_channel.h"

/* Two actors ONLY, zero-init once, externally publish completed init, stable
 * lifetime/address, stop/join BOTH before destruction. No live copy/move/reinit/
 * flush/reset/inner mutable APIs/private field access. Spans valid/disjoint and
 * exclusive to caller. One protocol actor owns actual RX; one driver actor owns
 * ingress producer. Driver never reads mutable protocol/private owner state.
 * Software publication barrier ONLY, not peripheral/DMA/IRQ/remote drain proof. */
typedef enum {TV4_GATE_OPEN,TV4_GATE_REQUESTED,TV4_GATE_PAUSED,TV4_GATE_BUSY,
    TV4_GATE_STALE,TV4_GATE_ARGUMENT,TV4_GATE_FAULT} tv4_ingress_gate_result;
struct tv4_ingress_owner;
typedef struct {const struct tv4_ingress_owner *owner;uint32_t epoch;} tv4_ingress_permit;
typedef struct {const struct tv4_ingress_owner *owner;uint32_t epoch;} tv4_ingress_pause;
typedef struct tv4_ingress_owner {
    tv4_frame_ingress ingress;
    tv4_channel_rx channel;
    atomic_uint gate,epoch;
    bool configured; /* immutable after init */
    bool failed,advanced; /* protocol actor ONLY */
} tv4_ingress_owner;
bool tv4_ingress_owner_init(tv4_ingress_owner *s,const tv4_fragment_config *fragments,
    unsigned capacity,uint32_t residency_ms,uint32_t route,uint64_t generation,
    uint16_t last_fragment,uint32_t last_serial,uint32_t last_epoch,uint32_t now);
/* Driver actor ONLY. Obtain permit BEFORE capture; retain it and original time
 * with BUSY external input. Never retag old input using a new epoch permit. */
tv4_ingress_gate_result tv4_ingress_owner_permit(tv4_ingress_owner *s,tv4_ingress_permit *out);
tv4_ingress_result tv4_ingress_owner_offer(tv4_ingress_owner *s,tv4_ingress_permit permit,
    uint32_t route,uint64_t generation,const tv4_fragment_frame *frame,uint32_t captured,uint32_t now);
tv4_ingress_result tv4_ingress_owner_retry(tv4_ingress_owner *s,uint32_t now);
tv4_ingress_gate_result tv4_ingress_owner_pause_request(tv4_ingress_owner *s,tv4_ingress_pause *out);
/* Exact request only, one actual pending retry max; BUSY requires consumer
 * drain then retry. Release ACK only after pending publication. No callbacks
 * that infer a current request, external drained bool, hardware stop assertion. */
tv4_ingress_gate_result tv4_ingress_owner_ack(tv4_ingress_owner *s,tv4_ingress_pause request,uint32_t now);
tv4_ingress_result tv4_ingress_owner_lost(tv4_ingress_owner *s);
/* Protocol actor ONLY. Poll without traffic. Stage then read fresh common time
 * BEFORE receive. receive feeds actual RX at CURRENT time, never captured time;
 * it does not expose raw payload. After RX failure, fresh checked owned copies
 * retire with explicit FRAG_STATE and NEVER feed RX. take is still UNVERIFIED. */
tv4_fragment_result tv4_ingress_owner_poll(tv4_ingress_owner *s,uint64_t generation,uint32_t now);
tv4_ingress_result tv4_ingress_owner_stage(tv4_ingress_owner *s,tv4_ingress_token *out);
tv4_fragment_result tv4_ingress_owner_receive(tv4_ingress_owner *s,tv4_ingress_token token,uint64_t generation,uint32_t now);
tv4_channel_result tv4_ingress_owner_take(tv4_ingress_owner *s,tv4_channel_message *out,uint64_t generation,uint32_t now);
tv4_ingress_gate_result tv4_ingress_owner_pause(tv4_ingress_owner *s,uint64_t generation,uint32_t now);
/* PAUSED requires actual driver ACK + ring/staged/pending drain + actual RX IDLE.
 * Still not upper negotiation/crypto/lease/model/image/close or physical proof. */
tv4_ingress_gate_result tv4_ingress_owner_quiescent(tv4_ingress_owner *s,uint64_t generation,uint32_t now);
/* One sequential phase advance per pause, explicit role ID; all higher protocol
 * gates remain caller obligations. No live clear or silent discard of partial RX. */
tv4_channel_result tv4_ingress_owner_advance(tv4_ingress_owner *s,tv4_channel_phase phase,
    tv4_update_id id,uint64_t generation,uint32_t now);
/* Only original lower fragment progress/total timeout; requires actual driver
 * ACK and ingress drain, no ingress fault. Retains phase/IDs/counters/clock/ring
 * positions; does not reopen gate or reset any upper image/session deadline.
 * Clock/context/exhaustion/loss/age and other failures cannot recover here. */
tv4_fragment_result tv4_ingress_owner_recover(tv4_ingress_owner *s,uint64_t generation,uint32_t now);
/* Requires ACK and actual ingress drain. Partial actual RX may resume SAME
 * phase unchanged; clocks never restart. Each resume issues fresh epoch, no
 * wrap/reset. Old captures/ACKs stay stale; sticky faults never reopen here. */
tv4_ingress_gate_result tv4_ingress_owner_resume(tv4_ingress_owner *s,uint64_t generation,uint32_t now);
#endif
