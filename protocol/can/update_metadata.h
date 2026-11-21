#ifndef TV4_UPDATE_METADATA_H
#define TV4_UPDATE_METADATA_H
#include "update_outcome.h"

/* EXPERIMENTAL raw metadata container inside a SEPARATE logical DATA stream.
 * One owner, zero-init once; no live copy/move/reinit, callbacks/heap/I/O/ISR.
 * All objects/spans valid, exclusive, disjoint. Public fields read-only after
 * init. Not a production CAN ABI/driver/identity provider or installer.
 * Metadata ID must be globally fresh in the selected boot epoch, distinct from
 * current AND historical image IDs. This API checks current pair and its own
 * metadata high-water; cross-role allocation/persistence/authentication external.
 * Container SHA in the DATA descriptor and raw metadata SHA in binding are
 * opaque expectations: transport/codec do NOT verify either digest/signature.
 * Never lend this metadata RX as an image RX to admission/handoff.
 * now: monotonic uint32 milliseconds modulo2^32; every gap<2^31.
 * Poll regularly without packets too; duplicate/reject/cached ACK never extends time. */
#define TV4_META_DOCUMENT_MAX 1024u
#define TV4_META_SIGNATURE_MAX 72u
#define TV4_META_KEY_MAX 31u
#define TV4_META_HEADER_BYTES 88u
#define TV4_META_BUNDLE_MAX (TV4_META_HEADER_BYTES+TV4_META_DOCUMENT_MAX+TV4_META_SIGNATURE_MAX+TV4_META_KEY_MAX)
#define TV4_META_BUNDLE_MIN (TV4_META_HEADER_BYTES+1u+8u+1u)
typedef struct {
    tv4_update_binding binding; /* expected negotiated IMAGE, not metadata DATA */
    uint16_t document_bytes,signature_bytes,key_id_bytes;
    uint8_t document[TV4_META_DOCUMENT_MAX];
    uint8_t signature[TV4_META_SIGNATURE_MAX];
    uint8_t key_id[TV4_META_KEY_MAX]; /* raw counted bytes; no text/DER policy */
} tv4_metadata_document;
typedef enum {
    TV4_META_OK, TV4_META_ARGUMENT, TV4_META_LENGTH, TV4_META_FORMAT,
    TV4_META_BINDING, TV4_META_IDENTITY, TV4_META_STATE,
    TV4_META_CONTEXT, TV4_META_CLOCK, TV4_META_TIMEOUT, TV4_META_STREAM,
    TV4_META_TAKEN_UNVERIFIED
} tv4_metadata_result;
typedef enum {
    TV4_META_IDLE, TV4_META_RECEIVING, TV4_META_COMPLETE_UNVERIFIED,
    TV4_META_TAKEN, TV4_META_FAILED
} tv4_metadata_state;
typedef struct {
    tv4_update_rx stream; /* private metadata role, inspect only */
    tv4_update_binding binding;
    tv4_update_limits limits;
    uint64_t generation;
    uint32_t boot,last_transfer,observed;
    tv4_metadata_state state;
    tv4_metadata_result error;
    bool configured;
    uint8_t bytes[TV4_META_BUNDLE_MAX];
} tv4_metadata_rx;

/* Header: 'TM',draft1,reserved0, lengths LE16(doc,sig,key),reserved0,0,
 * image boot/transfer/size LE32, image SHA32, expected raw metadata SHA32;
 * then exact counted key ID, signature, document. No padding/trailing bytes.
 * Encode failure leaves bytes untouched and written=0; decode failure zeroes out.
 * Success is structural only; callers must use the original independent
 * verifier/full image/live session/lease/physical/operator gates. */
tv4_metadata_result tv4_metadata_encode(const tv4_metadata_document *document,
    uint8_t *bytes,size_t capacity,size_t *written);
tv4_metadata_result tv4_metadata_decode(const uint8_t *bytes,size_t length,
    const tv4_update_binding *expected,tv4_metadata_document *out);
tv4_metadata_result tv4_metadata_rx_init(tv4_metadata_rx *rx,tv4_update_limits limits,
    uint32_t boot,uint64_t generation,uint32_t last_transfer,uint32_t now);
tv4_metadata_result tv4_metadata_rx_begin(tv4_metadata_rx *rx,
    const tv4_update_descriptor *metadata_transfer,const tv4_update_binding *expected_image,
    uint64_t generation,uint32_t now);
/* Uses unchanged CRC/session/sequence checks; ACK only after copy+consume.
 * ack is fully zero on non-ACK returns. Duplicates never copy/deliver twice. */
tv4_update_result tv4_metadata_rx_receive(tv4_metadata_rx *rx,const tv4_update_block *block,
    tv4_update_ack *ack,uint64_t generation,uint32_t now);
tv4_metadata_result tv4_metadata_rx_poll(tv4_metadata_rx *rx,uint64_t generation,uint32_t now);
/* Copies a single owned snapshot; no borrowed pointers. Its bytes are still
 * UNVERIFIED, not a live receipt/install token. It must remain exclusive/alive
 * until the existing handoff snapshots it; signature/raw digest checks mandatory.
 * Wipes aggregate bytes, retains last DATA block only for bounded final ACKs. */
tv4_metadata_result tv4_metadata_rx_take(tv4_metadata_rx *rx,tv4_metadata_document *out,
    uint64_t generation,uint32_t now);
/* Explicit cancellation/recovery, wipes aggregate/cache, retains counter/time.
 * CLOCK/CONTEXT remain sticky. Does not touch the separate image RX/deadlines. */
tv4_metadata_result tv4_metadata_rx_discard(tv4_metadata_rx *rx,uint64_t generation,uint32_t now);
#endif
