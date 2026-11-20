#include "metadata.h"
#include "release_metadata.h"
#include <string.h>

typedef struct { const uint8_t *p, *end; } parser;
typedef struct { const uint8_t *p; size_t length; } span;
static bool id_byte(uint8_t b)
{
    return (b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') ||
           (b >= '0' && b <= '9') || b == '.' || b == '-' || b == '_';
}
bool tv4_boot_identifier(const char value[TV4_BOOT_ID_BYTES])
{
    for (size_t i = 0; i < TV4_BOOT_ID_BYTES; ++i) {
        if (value[i] == '\0') return i != 0;
        if (!id_byte((uint8_t)value[i])) return false;
    }
    return false;
}
static void space(parser *p)
{
    while (p->p != p->end && (*p->p == ' ' || *p->p == '\t' || *p->p == '\n' || *p->p == '\r')) ++p->p;
}
static bool token(parser *p, uint8_t byte)
{
    space(p);
    if (p->p == p->end || *p->p != byte) return false;
    ++p->p;
    return true;
}
static bool string(parser *p, span *out)
{
    if (!token(p, '"')) return false;
    out->p = p->p;
    while (p->p != p->end && *p->p != '"') {
        /* Draft has ASCII machine identifiers only; no escapes/embedded NUL. */
        if (!id_byte(*p->p)) return false;
        ++p->p;
    }
    out->length = (size_t)(p->p - out->p);
    if (out->length == 0 || out->length > 64 || p->p == p->end) return false;
    ++p->p;
    return true;
}
static bool equal(span s, const char *literal)
{
    size_t n = strlen(literal);
    return s.length == n && memcmp(s.p, literal, n) == 0;
}
static bool bounded_string(parser *p, char *out, size_t capacity)
{
    span s;
    if (!string(p, &s) || s.length >= capacity) return false;
    memcpy(out, s.p, s.length);
    out[s.length] = '\0';
    return true;
}
static bool identifier(parser *p, char out[TV4_BOOT_ID_BYTES])
{
    return bounded_string(p, out, TV4_BOOT_ID_BYTES);
}
static bool integer(parser *p, uint32_t *out)
{
    space(p);
    if (p->p == p->end || *p->p < '0' || *p->p > '9') return false;
    bool zero = *p->p == '0';
    uint32_t n = (uint32_t)(*p->p++ - '0');
    while (p->p != p->end && *p->p >= '0' && *p->p <= '9') {
        uint32_t digit = (uint32_t)(*p->p++ - '0');
        if (zero || n > (UINT32_MAX - digit) / 10u) return false;
        n = n * 10u + digit;
    }
    *out = n;
    return true;
}
static bool digest(parser *p, uint8_t out[32])
{
    span s;
    if (!string(p, &s) || s.length != 64) return false;
    for (size_t i = 0; i < 32; ++i) {
        uint8_t pair = 0;
        for (size_t j = 0; j < 2; ++j) {
            uint8_t b = s.p[i * 2 + j];
            unsigned digit;
            if (b >= '0' && b <= '9') digit = (unsigned)(b - '0');
            else if (b >= 'a' && b <= 'f') digit = (unsigned)(b - 'a') + 10u;
            else return false;
            pair = (uint8_t)((unsigned)pair * 16u + digit);
        }
        out[i] = pair;
    }
    return true;
}
static bool parse(const uint8_t *bytes, size_t length, tv4_boot_metadata *out,
    tv4_boot_release_candidate *extra)
{
    static const char *const fields[] = {"schema", "component", "hardware_id", "hardware_revision",
        "memory_profile", "image_counter", "minimum_bootloader_counter", "protocol_min",
        "protocol_max", "image_size", "sha256", "release_version", "component_version", "channel", "source_commit"};
    if (bytes == NULL || out == NULL || length == 0 || length > TV4_BOOT_METADATA_MAX) return false;
    parser p = {bytes, bytes + length};
    tv4_boot_metadata m = {0};
    unsigned seen = 0;
    unsigned count = extra == NULL ? 11u : 15u;
    if (!token(&p, '{')) return false;
    for (unsigned field = 0; field < count; ++field) {
        span key, value;
        if (field != 0 && !token(&p, ',')) return false;
        if (!string(&p, &key) || !token(&p, ':')) return false;
        unsigned index = 0;
        while (index < count && !equal(key, fields[index])) ++index;
        if (index == count || (seen & (1u << index)) != 0) return false;
        seen |= 1u << index;
        bool ok;
        switch (index) {
        case 0: ok = string(&p, &value) && equal(value, extra == NULL ? TV4_BOOT_DRAFT_SCHEMA : TV4_BOOT_RELEASE_SCHEMA); break;
        case 1: ok = string(&p, &value) && equal(value, "motion"); break;
        case 2: ok = identifier(&p, m.hardware_id); break;
        case 3: ok = identifier(&p, m.hardware_revision); break;
        case 4: ok = identifier(&p, m.memory_profile); break;
        case 5: ok = integer(&p, &m.image_counter); break;
        case 6: ok = integer(&p, &m.minimum_bootloader_counter); break;
        case 7: ok = integer(&p, &m.protocol_min); break;
        case 8: ok = integer(&p, &m.protocol_max); break;
        case 9: ok = integer(&p, &m.image_size); break;
        case 10: ok = digest(&p, m.sha256); break;
        case 11: ok = extra != NULL && bounded_string(&p, extra->release_version, TV4_BOOT_VERSION_BYTES); break;
        case 12: ok = extra != NULL && bounded_string(&p, extra->component_version, TV4_BOOT_VERSION_BYTES); break;
        case 13: ok = extra != NULL && bounded_string(&p, extra->channel, TV4_BOOT_CHANNEL_BYTES); break;
        default: ok = extra != NULL && bounded_string(&p, extra->source_commit, TV4_BOOT_COMMIT_BYTES); break;
        }
        if (!ok) return false;
    }
    if (seen != (1u << count) - 1u || !token(&p, '}')) return false;
    space(&p);
    if (p.p != p.end) return false;
    if (extra != NULL && !tv4_boot_release_fields_valid(extra)) return false;
    *out = m;
    return true;
}
bool tv4_boot_parse_metadata(const uint8_t *bytes, size_t length, tv4_boot_metadata *out)
{
    return parse(bytes, length, out, NULL);
}
bool tv4_boot_parse_release_metadata(const uint8_t *bytes, size_t length,
    tv4_boot_metadata *out, tv4_boot_release_candidate *extra)
{
    return extra != NULL && parse(bytes, length, out, extra);
}
