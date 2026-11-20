#include "image_profile.h"
#include "metadata.h"
#include <string.h>

static bool separate(const void *a, size_t as, const void *b, size_t bs)
{
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return a != NULL && b != NULL && as <= UINTPTR_MAX - x && bs <= UINTPTR_MAX - y &&
        (x + as <= y || y + bs <= x);
}
static uint64_t end(tv4_boot_ram_span span) { return (uint64_t)span.start + span.bytes; }
static bool reserved(const tv4_boot_image_profile *p, unsigned index)
{
    if (index < 16u) return (index >= 7u && index <= 10u) || index == 13u;
    unsigned irq = index - 16u;
    return ((p->reserved_irq[irq / 32u] >> (irq % 32u)) & 1u) != 0;
}
bool tv4_boot_image_profile_valid(const tv4_boot_image_profile *p)
{
    if (p == NULL || !tv4_boot_identifier(p->id) || p->image_base == 0 || p->image_capacity == 0 ||
        (uint64_t)p->image_base + p->image_capacity > (UINT64_C(1) << 32) ||
        p->vector_count < 16u || p->vector_count > TV4_BOOT_VECTORS_MAX ||
        p->ram_count == 0 || p->ram_count > TV4_BOOT_RAM_SPANS_MAX ||
        p->minimum_stack_bytes == 0 || p->minimum_stack_bytes % 8u != 0) return false;
    uint32_t table = (uint32_t)p->vector_count * 4u, alignment = 128u;
    while (alignment < table) alignment *= 2u;
    /* Architectural minimum is NOT a device-specific VTOR contract. E.g.
     * PM0214 Rev10 specifies 128 words; the generic ARM guide uses 32 words.
     * Require an explicit local alignment reviewed for the actual target. */
    if (p->vector_alignment < alignment || (p->vector_alignment & (p->vector_alignment - 1u)) != 0 ||
        p->image_base % p->vector_alignment != 0 || p->image_capacity < table + 2u) return false;
    bool stack_fits = false;
    for (unsigned i = 0; i < TV4_BOOT_RAM_SPANS_MAX; ++i) {
        tv4_boot_ram_span r = p->ram[i];
        if (i >= p->ram_count) { if (r.start != 0 || r.bytes != 0) return false; continue; }
        if (r.start == 0 || r.bytes == 0 || end(r) > (UINT64_C(1) << 32) ||
            ((uint64_t)r.start < (uint64_t)p->image_base + p->image_capacity && p->image_base < end(r))) return false;
        if (r.bytes >= p->minimum_stack_bytes) stack_fits = true;
        for (unsigned j = 0; j < i; ++j)
            if ((uint64_t)r.start < end(p->ram[j]) && p->ram[j].start < end(r)) return false;
    }
    for (unsigned irq = p->vector_count - 16u; irq < TV4_BOOT_IRQ_MASK_WORDS * 32u; ++irq)
        if (((p->reserved_irq[irq / 32u] >> (irq % 32u)) & 1u) != 0) return false;
    return stack_fits;
}
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
tv4_boot_image_result tv4_boot_inspect_vectors(const tv4_boot_image_profile *p,
    const uint8_t *bytes, size_t size, uint32_t image_bytes, tv4_boot_vector_inspection *out)
{
    if (out == NULL) return TV4_IMAGE_ARGUMENT;
    if ((p != NULL && !separate(out, sizeof(*out), p, sizeof(*p))) ||
        (bytes != NULL && size != 0 && !separate(out, sizeof(*out), bytes, size)))
        return TV4_IMAGE_ARGUMENT;
    *out = (tv4_boot_vector_inspection){0};
    if (p == NULL || bytes == NULL) return TV4_IMAGE_ARGUMENT;
    if (!tv4_boot_image_profile_valid(p)) return TV4_IMAGE_PROFILE;
    uint32_t table = (uint32_t)p->vector_count * 4u;
    if (size != table || image_bytes < table + 2u || image_bytes > p->image_capacity) return TV4_IMAGE_LENGTH;
    uint32_t sp = le32(bytes);
    bool stack = false;
    if (sp % 8u == 0 && sp >= p->minimum_stack_bytes)
        for (unsigned i = 0; i < p->ram_count; ++i)
            if (sp - p->minimum_stack_bytes >= p->ram[i].start && (uint64_t)sp <= end(p->ram[i])) stack = true;
    if (!stack) return TV4_IMAGE_STACK;
    for (unsigned i = 1; i < p->vector_count; ++i) {
        uint32_t word = le32(bytes + i * 4u);
        if (reserved(p, i)) { if (word != 0) return TV4_IMAGE_RESERVED_VECTOR; continue; }
        uint32_t pc = word & ~UINT32_C(1);
        if ((word & 1u) == 0 || (uint64_t)pc < (uint64_t)p->image_base + table ||
            (uint64_t)pc + 2u > (uint64_t)p->image_base + image_bytes) return TV4_IMAGE_HANDLER;
    }
    out->initial_sp = sp; out->reset_handler = le32(bytes + 4);
    out->vector_count = p->vector_count;
    memcpy(out->profile_id, p->id, strlen(p->id) + 1u);
    return TV4_IMAGE_PROFILE_MODEL_MATCH;
}
