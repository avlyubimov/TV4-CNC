#include "image_profile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool zero(const void *p, size_t n)
{
    const uint8_t *bytes = p;
    for (size_t i = 0; i < n; ++i) if (bytes[i] != 0) return false;
    return true;
}
static void word(uint8_t *out, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) out[i] = (uint8_t)(v >> (i * 8u));
}
static bool reserved(const tv4_boot_image_profile *p, unsigned v)
{
    if (v < 16) return (v >= 7 && v <= 10) || v == 13;
    unsigned irq = v - 16;
    return ((p->reserved_irq[irq / 32u] >> (irq % 32u)) & 1u) != 0;
}
static tv4_boot_image_profile profile(unsigned vectors)
{
    tv4_boot_image_profile p = {.id = "synthetic-vector-v1", .image_base = 0x01000000,
        .image_capacity = 65536, .minimum_stack_bytes = 128, .vector_alignment = 1024,
        .vector_count = (uint16_t)vectors, .ram_count = 2,
        .ram = {{0x30000000, 4096}, {0x30010000, 2048}}};
    for (unsigned irq = 0; irq + 16u < vectors; ++irq)
        if (irq % 5u == 3u) p.reserved_irq[irq / 32u] |= UINT32_C(1) << (irq % 32u);
    return p;
}
static void table(const tv4_boot_image_profile *p, uint8_t bytes[TV4_BOOT_VECTOR_BYTES_MAX])
{
    memset(bytes, 0, TV4_BOOT_VECTOR_BYTES_MAX);
    word(bytes, p->ram[0].start + p->ram[0].bytes);
    for (unsigned i = 1; i < p->vector_count; ++i)
        word(bytes + i * 4u, reserved(p, i) ? 0 : (p->image_base + (uint32_t)p->vector_count * 4u) | 1u);
}
static tv4_boot_image_result inspect(tv4_boot_image_profile *p, uint8_t *bytes, uint32_t length)
{
    tv4_boot_vector_inspection out;
    memset(&out, 0xa5, sizeof(out));
    tv4_boot_image_result r = tv4_boot_inspect_vectors(p, bytes, (size_t)p->vector_count * 4u, length, &out);
    if (r != TV4_IMAGE_PROFILE_MODEL_MATCH) assert(zero(&out, sizeof(out)));
    else { assert(out.vector_count == p->vector_count); assert(strcmp(out.profile_id, p->id) == 0); }
    return r;
}
static void all_table_lengths(void)
{
    for (unsigned count = 16; count <= 256; ++count) {
        tv4_boot_image_profile p = profile(count); uint8_t bytes[TV4_BOOT_VECTOR_BYTES_MAX]; table(&p, bytes);
        assert(tv4_boot_image_profile_valid(&p));
        assert(inspect(&p, bytes, count * 4u + 2u) == TV4_IMAGE_PROFILE_MODEL_MATCH);
        assert(inspect(&p, bytes, count * 4u + 1u) == TV4_IMAGE_LENGTH);
    }
    puts("PASS 241 vector-table lengths, 16..256 entries; synthetic models only");
}
static void unknown_and_invalid_profiles(void)
{
    assert(!tv4_boot_image_profile_valid(NULL));
    tv4_boot_image_profile empty = {0}; assert(!tv4_boot_image_profile_valid(&empty));
    for (unsigned fault = 0; fault < 21; ++fault) {
        tv4_boot_image_profile p = profile(32);
        switch (fault) {
        case 0: p.image_base = 0; break;
        case 1: p.image_capacity = 0; break;
        case 2: p.image_capacity = 129; break;
        case 3: p.minimum_stack_bytes = 0; break;
        case 4: p.minimum_stack_bytes = 129; break;
        case 5: p.minimum_stack_bytes = 65536; break;
        case 6: p.vector_alignment = 0; break;
        case 7: p.vector_alignment = 64; break;
        case 8: p.vector_alignment = 768; break;
        case 9: p.image_base += 128; break;
        case 10: p.vector_count = 15; break;
        case 11: p.vector_count = 257; break;
        case 12: p.ram_count = 0; break;
        case 13: p.ram_count = 5; break;
        case 14: p.ram[1] = p.ram[0]; break;
        case 15: p.ram[0].start = p.image_base; break;
        case 16: p.ram[0].bytes = 0; break;
        case 17: p.ram[2].start = 0x30020000; break;
        case 18: p.reserved_irq[0] |= UINT32_C(1) << 16; break; /* first unused IRQ */
        case 19: memset(p.id, 'x', sizeof(p.id)); break;
        default: p.id[0] = '!'; break;
        }
        assert(!tv4_boot_image_profile_valid(&p));
    }
    tv4_boot_image_profile p = profile(33); p.vector_alignment = 128;
    assert(!tv4_boot_image_profile_valid(&p)); /* table exceeds generic minimum */
}
static void stack_edges_and_gaps(void)
{
    tv4_boot_image_profile p = profile(32); uint8_t bytes[TV4_BOOT_VECTOR_BYTES_MAX]; table(&p, bytes);
    uint32_t bad[] = {0, 1, p.ram[0].start, p.ram[0].start + 120u, p.ram[0].start + 4097u,
        p.ram[0].start + 4104u, p.ram[1].start + 120u, UINT32_MAX};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        word(bytes, bad[i]); assert(inspect(&p, bytes, 1024) == TV4_IMAGE_STACK);
    }
    uint32_t good[] = {p.ram[0].start + 128u, p.ram[0].start + 4096u, p.ram[1].start + 2048u};
    for (size_t i = 0; i < sizeof(good) / sizeof(good[0]); ++i) {
        word(bytes, good[i]); assert(inspect(&p, bytes, 1024) == TV4_IMAGE_PROFILE_MODEL_MATCH);
    }
    p.ram[1].start = p.ram[0].start + p.ram[0].bytes; /* adjacent, not one stack bank */
    word(bytes, p.ram[1].start + 64u);
    assert(tv4_boot_image_profile_valid(&p));
    assert(inspect(&p, bytes, 1024) == TV4_IMAGE_STACK);
}
static void every_handler_and_reserved_slot(void)
{
    tv4_boot_image_profile p = profile(256); uint8_t bytes[TV4_BOOT_VECTOR_BYTES_MAX];
    for (unsigned i = 1; i < p.vector_count; ++i) {
        table(&p, bytes);
        if (reserved(&p, i)) {
            word(bytes + i * 4u, p.image_base + 1025u);
            assert(inspect(&p, bytes, 2048) == TV4_IMAGE_RESERVED_VECTOR);
        } else {
            uint32_t bad[] = {0, UINT32_MAX, p.image_base + 1024u, p.image_base + 1u,
                p.image_base - 1u, p.image_base + 2049u, p.ram[0].start | 1u};
            for (size_t j = 0; j < sizeof(bad) / sizeof(bad[0]); ++j) {
                word(bytes + i * 4u, bad[j]);
                assert(inspect(&p, bytes, 2048) == TV4_IMAGE_HANDLER);
            }
            word(bytes + i * 4u, p.image_base + 2047u); /* last full halfword */
            assert(inspect(&p, bytes, 2048) == TV4_IMAGE_PROFILE_MODEL_MATCH);
            assert(inspect(&p, bytes, 2047) == TV4_IMAGE_HANDLER);
        }
    }
    puts("PASS every slot in 256-entry table; null/even/outside/table/RAM/truncated Thumb target rejected");
}
static void full_uint32_arithmetic(void)
{
    /* Numeric overflow tests only: NOT a Cortex-M4 executable/VTOR region. */
    tv4_boot_image_profile p = profile(17); uint8_t bytes[TV4_BOOT_VECTOR_BYTES_MAX];
    p.image_base = UINT32_C(0xfffffc00); p.image_capacity = 1024;
    table(&p, bytes); assert(inspect(&p, bytes, 1024) == TV4_IMAGE_PROFILE_MODEL_MATCH);
    ++p.image_capacity; assert(!tv4_boot_image_profile_valid(&p));
    p = profile(17); p.ram[1] = (tv4_boot_ram_span){UINT32_C(0xfffff800),2048};
    table(&p, bytes); word(bytes, UINT32_C(0xfffffff8));
    assert(inspect(&p, bytes, 1024) == TV4_IMAGE_PROFILE_MODEL_MATCH);
    ++p.ram[1].bytes; assert(!tv4_boot_image_profile_valid(&p));
}
static void lengths_aliases_and_zero_output(void)
{
    tv4_boot_image_profile p = profile(32); uint8_t bytes[TV4_BOOT_VECTOR_BYTES_MAX]; table(&p, bytes);
    tv4_boot_vector_inspection out;
    memset(&out, 0xa5, sizeof(out));
    assert(tv4_boot_inspect_vectors(&p, bytes, 127, 1024, &out) == TV4_IMAGE_LENGTH && zero(&out, sizeof(out)));
    assert(inspect(&p, bytes, 65537) == TV4_IMAGE_LENGTH);
    assert(tv4_boot_inspect_vectors(NULL, bytes, 128, 1024, &out) == TV4_IMAGE_ARGUMENT);
    assert(tv4_boot_inspect_vectors(&p, NULL, 128, 1024, &out) == TV4_IMAGE_ARGUMENT);
    assert(tv4_boot_inspect_vectors(&p, bytes, 128, 1024, NULL) == TV4_IMAGE_ARGUMENT);
    tv4_boot_image_profile before = p;
    assert(tv4_boot_inspect_vectors(&p, bytes, 128, 1024, (tv4_boot_vector_inspection *)(void *)&p) == TV4_IMAGE_ARGUMENT);
    assert(memcmp(&p, &before, sizeof(p)) == 0);
    size_t used = strlen(p.id) + 1u;
    memset(p.id + used, 0xa5, sizeof(p.id) - used);
    assert(tv4_boot_inspect_vectors(&p,bytes,128,1024,&out) == TV4_IMAGE_PROFILE_MODEL_MATCH);
    assert(zero(out.profile_id + used,sizeof(out.profile_id) - used));
}
#define RUN(name) do { name(); puts("PASS " #name); } while (0)
int main(void)
{
    RUN(all_table_lengths); RUN(unknown_and_invalid_profiles); RUN(stack_edges_and_gaps);
    RUN(every_handler_and_reserved_slot); RUN(full_uint32_arithmetic); RUN(lengths_aliases_and_zero_output);
    printf("PASS 6 profile groups; profile=%zu inspection=%zu; model consistency only, no execution/device access\n",
        sizeof(tv4_boot_image_profile), sizeof(tv4_boot_vector_inspection));
    return 0;
}
