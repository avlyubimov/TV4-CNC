#pragma once
#include "manifest.h"
#include "hash.h"
#define TV4_OTA_CHUNK_MAX 4096u
/* Read-only stream verifier. No sink, Flash, boot selection or installer API.
 * VERIFIED means streamed integrity/format checks, not SDK bootability/approval. */
typedef enum { TV4_IMAGE_IDLE,TV4_IMAGE_RECEIVING,TV4_IMAGE_VERIFIED,
               TV4_IMAGE_FAILED,TV4_IMAGE_CANCELLED } tv4_image_state;
typedef enum { TV4_IMAGE_NONE,TV4_IMAGE_ARGUMENT,TV4_IMAGE_CRYPTO,
    TV4_IMAGE_TIMEOUT,TV4_IMAGE_LENGTH,TV4_IMAGE_HEADER,TV4_IMAGE_SEGMENT,
    TV4_IMAGE_DESCRIPTOR,TV4_IMAGE_CHECKSUM,TV4_IMAGE_DIGEST,TV4_IMAGE_IO } tv4_image_error;
typedef struct {
    uint32_t idle_timeout_ms,total_timeout_ms;
    uint16_t chip_revision; /* Actual major*100+minor from local context. */
    bool chip_revision_known;
} tv4_image_limits;
typedef struct {
    tv4_image_state state;tv4_image_error error;
    uint32_t expected,received,started_ms,progress_ms;
    tv4_image_limits limits;
    tv4_ota_hash whole_hash,body_hash;
    uint8_t expected_digest[32],body_digest[32],tail[32];
    uint8_t header[24],segment[8],descriptor[256];
    char version[32];
    uint32_t segment_left,padding_left;
    unsigned phase,used,index,segments,descriptor_used;
    uint8_t checksum;
} tv4_image_stream;
/* Zero-initialize once. Only pass candidate from authenticated native policy.
 * Unknown real chip revision fails closed. Close before reuse/retry; no resume. */
bool tv4_image_start(tv4_image_stream *s,const tv4_ota_candidate *candidate,
                     tv4_image_limits limits,uint32_t now_ms);
/* Immutable borrowed bytes during call; 1..4096, sequential, no retained pointer.
 * Check deadlines BEFORE accepting progress, including late arriving chunks. */
bool tv4_image_feed(tv4_image_stream *s,const uint8_t *bytes,size_t length,uint32_t now_ms);
bool tv4_image_poll(tv4_image_stream *s,uint32_t now_ms);
bool tv4_image_finish(tv4_image_stream *s,uint32_t now_ms);
void tv4_image_abort(tv4_image_stream *s,bool cancelled);
void tv4_image_close(tv4_image_stream *s);
