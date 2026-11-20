#ifndef TV4_UPDATE_SESSION_H
#define TV4_UPDATE_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Logical software messages, NOT a CAN wire format or production protocol.
 * One owner, initialized objects, no ISR/reentrancy. No heap, I/O, Flash,
 * authentication, physical safe-state assertion, commit or jump is provided. */
#define TV4_UPDATE_BLOCK_MAX 256u
#define TV4_UPDATE_HASH_BYTES 32u

typedef struct { uint32_t boot, transfer; } tv4_update_id;
typedef struct {
    tv4_update_id id;
    uint32_t image_bytes;
    /* Opaque identity copied from the caller; NOT a verified digest. */
    uint8_t image_sha256[TV4_UPDATE_HASH_BYTES];
} tv4_update_descriptor;
typedef struct {
    uint32_t max_image_bytes;
    uint32_t ack_timeout_ms, progress_timeout_ms, total_timeout_ms;
    uint16_t block_bytes;
    uint8_t retry_limit;
} tv4_update_limits;
typedef struct {
    tv4_update_id id;
    uint32_t sequence, offset;
    uint16_t length;
    uint32_t crc32c;
    uint8_t bytes[TV4_UPDATE_BLOCK_MAX];
} tv4_update_block;
typedef struct {
    tv4_update_id id;
    uint32_t next_sequence, next_offset;
} tv4_update_ack;

typedef enum {
    TV4_UPDATE_IDLE, TV4_UPDATE_RECEIVING, TV4_UPDATE_BLOCK_PENDING,
    TV4_UPDATE_NEED_DATA, TV4_UPDATE_TX_READY, TV4_UPDATE_WAIT_ACK,
    TV4_UPDATE_COMPLETE_UNVERIFIED, TV4_UPDATE_ABORTED, TV4_UPDATE_FAILED
} tv4_update_state;
typedef enum {
    TV4_UPDATE_ERROR_NONE, TV4_UPDATE_ERROR_CONFIG, TV4_UPDATE_ERROR_CLOCK,
    TV4_UPDATE_ERROR_PROGRESS_TIMEOUT, TV4_UPDATE_ERROR_TOTAL_TIMEOUT,
    TV4_UPDATE_ERROR_RETRIES, TV4_UPDATE_ERROR_CONSUMER
} tv4_update_error;
typedef enum {
    TV4_UPDATE_OK, TV4_UPDATE_PENDING, TV4_UPDATE_ACK, TV4_UPDATE_WAIT,
    TV4_UPDATE_REJECT_ARGUMENT, TV4_UPDATE_REJECT_STATE,
    TV4_UPDATE_REJECT_SESSION, TV4_UPDATE_REJECT_SIZE,
    TV4_UPDATE_REJECT_ORDER, TV4_UPDATE_REJECT_CHECKSUM,
    TV4_UPDATE_REJECT_CONTENT, TV4_UPDATE_STALE_ACK, TV4_UPDATE_ERROR
} tv4_update_result;

/* Public storage for static allocation/inspection; do not mutate live state. */
typedef struct {
    tv4_update_limits limits;
    tv4_update_descriptor descriptor;
    tv4_update_state state;
    tv4_update_error error;
    uint32_t local_boot, started_ms, progress_ms, observed_ms;
    uint32_t next_sequence, next_offset;
    uint8_t retries;
    bool configured;
} tv4_update_context;
typedef struct { tv4_update_context context; tv4_update_block block; bool retained; } tv4_update_rx;
typedef struct { tv4_update_context context; tv4_update_block block; uint32_t sent_ms; } tv4_update_tx;

/* Limits have no defaults: 0 < ack < progress <= total <= INT32_MAX;
 * block 1..256, nonzero max image/retry/boot. A boot/transfer identity must
 * never be reused for a different negotiation (uniqueness is external).
 * now_ms is monotonic modulo 2^32; every interval must be <2^31 ms.
 * Poll regularly even without packets; duplicates/rejects do not make progress.
 * Begin is local, transport-only admission, NOT an install/quiescent gate. */
bool tv4_update_rx_init(tv4_update_rx *rx, tv4_update_limits limits,
                        uint32_t local_boot, uint32_t now_ms);
bool tv4_update_tx_init(tv4_update_tx *tx, tv4_update_limits limits,
                        uint32_t local_boot, uint32_t now_ms);
tv4_update_result tv4_update_rx_begin(tv4_update_rx *rx,
    const tv4_update_descriptor *descriptor, uint32_t now_ms);
tv4_update_result tv4_update_tx_begin(tv4_update_tx *tx,
    const tv4_update_descriptor *descriptor, uint32_t now_ms);
tv4_update_result tv4_update_rx_poll(tv4_update_rx *rx, uint32_t now_ms);
tv4_update_result tv4_update_tx_poll(tv4_update_tx *tx, uint32_t now_ms);

/* CRC-32C (Castagnoli) is accidental-corruption detection, not a signature.
 * Raw helper is limited to 256 bytes (NULL is allowed only for length zero).
 * Block checksum covers boot/transfer/sequence/offset LE32, length LE16,
 * then exactly length payload bytes; no C padding or unused bytes. */
bool tv4_update_crc32c(const uint8_t *bytes, size_t length, uint32_t *checksum);
bool tv4_update_block_checksum(const tv4_update_block *block, uint32_t *checksum);

/* Receive copies the block into fixed storage. PENDING is not an ACK.
 * An exact retained duplicate gets ACK only after consumer success.
 * ack/out are written only when the returned result warrants their use. */
tv4_update_result tv4_update_rx_receive(tv4_update_rx *rx,
    const tv4_update_block *block, tv4_update_ack *ack, uint32_t now_ms);
tv4_update_result tv4_update_rx_pending(tv4_update_rx *rx,
    tv4_update_block *out, uint32_t now_ms);
tv4_update_result tv4_update_rx_consume(tv4_update_rx *rx, tv4_update_id id,
    uint32_t sequence, bool consumer_ok, tv4_update_ack *ack, uint32_t now_ms);

/* Load is only a bounded memory copy; storage reads belong to a future owner.
 * Next returns an identical cached block initially and on bounded retries.
 * ACK only confirms the transport cursor; it cannot verify/activate an image. */
tv4_update_result tv4_update_tx_load(tv4_update_tx *tx,
    const uint8_t *bytes, size_t length, uint32_t now_ms);
tv4_update_result tv4_update_tx_next(tv4_update_tx *tx,
    tv4_update_block *out, uint32_t now_ms);
tv4_update_result tv4_update_tx_ack(tv4_update_tx *tx,
    const tv4_update_ack *ack, uint32_t now_ms);
tv4_update_result tv4_update_rx_abort(tv4_update_rx *rx, tv4_update_id id, uint32_t now_ms);
tv4_update_result tv4_update_tx_abort(tv4_update_tx *tx, tv4_update_id id, uint32_t now_ms);

#endif
