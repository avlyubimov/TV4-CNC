#include "metadata.h"
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
static bool identifier(parser *p, char out[TV4_BOOT_ID_BYTES])
{
    span s;
    if (!string(p, &s) || s.length >= TV4_BOOT_ID_BYTES) return false;
    memcpy(out, s.p, s.length);
    out[s.length] = '\0';
    return true;
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
bool tv4_boot_parse_metadata(const uint8_t *bytes, size_t length, tv4_boot_metadata *out)
{
    static const char *const fields[] = {"schema", "component", "hardware_id", "hardware_revision",
        "memory_profile", "image_counter", "minimum_bootloader_counter", "protocol_min",
        "protocol_max", "image_size", "sha256"};
    if (bytes == NULL || out == NULL || length == 0 || length > TV4_BOOT_METADATA_MAX) return false;
    parser p = {bytes, bytes + length};
    tv4_boot_metadata m = {0};
    unsigned seen = 0;
    if (!token(&p, '{')) return false;
    for (unsigned field = 0; field < 11; ++field) {
        span key, value;
        if (field != 0 && !token(&p, ',')) return false;
        if (!string(&p, &key) || !token(&p, ':')) return false;
        unsigned index = 0;
        while (index < 11 && !equal(key, fields[index])) ++index;
        if (index == 11 || (seen & (1u << index)) != 0) return false;
        seen |= 1u << index;
        bool ok;
        switch (index) {
        case 0: ok = string(&p, &value) && equal(value, TV4_BOOT_DRAFT_SCHEMA); break;
        case 1: ok = string(&p, &value) && equal(value, "motion"); break;
        case 2: ok = identifier(&p, m.hardware_id); break;
        case 3: ok = identifier(&p, m.hardware_revision); break;
        case 4: ok = identifier(&p, m.memory_profile); break;
        case 5: ok = integer(&p, &m.image_counter); break;
        case 6: ok = integer(&p, &m.minimum_bootloader_counter); break;
        case 7: ok = integer(&p, &m.protocol_min); break;
        case 8: ok = integer(&p, &m.protocol_max); break;
        case 9: ok = integer(&p, &m.image_size); break;
        default: ok = digest(&p, m.sha256); break;
        }
        if (!ok) return false;
    }
    if (seen != 0x7ffu || !token(&p, '}')) return false;
    space(&p);
    if (p.p != p.end) return false;
    *out = m;
    return true;
}
