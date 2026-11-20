#ifndef TV4_BOOT_IMAGE_PROFILE_H
#define TV4_BOOT_IMAGE_PROFILE_H

#include "admission.h"

#define TV4_BOOT_RAM_SPANS_MAX 4u
#define TV4_BOOT_VECTORS_MAX 256u
#define TV4_BOOT_VECTOR_BYTES_MAX (TV4_BOOT_VECTORS_MAX * 4u)
#define TV4_BOOT_IRQ_MASK_WORDS 8u

typedef struct { uint32_t start, bytes; } tv4_boot_ram_span;
/* LOCAL model only, never populated from a peer/untrusted manifest. No default
 * MCU map, Flash sector reservation, actual RAM/erase/protection proof or writer.
 * Vectors are little-endian words at image offset zero. Unused RAM entries and
 * IRQ-mask bits outside vector_count MUST be zero. Change identity/generation
 * when any mapping rule changes. Real device vector count/reserved IRQs unknown.
 */
typedef struct {
    char id[TV4_BOOT_ID_BYTES];
    uint32_t image_base, image_capacity, minimum_stack_bytes, vector_alignment;
    uint16_t vector_count;
    uint8_t ram_count;
    tv4_boot_ram_span ram[TV4_BOOT_RAM_SPANS_MAX];
    uint32_t reserved_irq[TV4_BOOT_IRQ_MASK_WORDS];
} tv4_boot_image_profile;
typedef enum {
    TV4_IMAGE_PROFILE_MODEL_MATCH, TV4_IMAGE_ARGUMENT, TV4_IMAGE_PROFILE,
    TV4_IMAGE_LENGTH, TV4_IMAGE_STACK, TV4_IMAGE_RESERVED_VECTOR,
    TV4_IMAGE_HANDLER, TV4_IMAGE_CONTEXT, TV4_IMAGE_SOURCE, TV4_IMAGE_BUSY
} tv4_boot_image_result;
typedef struct {
    uint32_t initial_sp, reset_handler;
    uint16_t vector_count;
    char profile_id[TV4_BOOT_ID_BYTES];
} tv4_boot_vector_inspection;

bool tv4_boot_image_profile_valid(const tv4_boot_image_profile *profile);
/* Pure structural check, NOT authentication or permission to execute/write.
 * Supply exactly vector_count*4 bytes, with actual authenticated image length
 * separately. Zero output on failure except illegal aliases (no writes).
 * Strict project policy: SP aligned8 with minimum stack in ONE RAM span, all
 * non-reserved handlers Thumb and within image bytes AFTER the vector table.
 * No address dereference, FPU/stack/IRQ initialization or hardware operation.
 */
tv4_boot_image_result tv4_boot_inspect_vectors(const tv4_boot_image_profile *profile,
    const uint8_t *vectors, size_t vector_bytes, uint32_t image_bytes,
    tv4_boot_vector_inspection *out);

#endif
