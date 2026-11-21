#include "release_metadata.h"
#include <string.h>

typedef struct { uint32_t core[3]; const char *pre, *end; } version;
static bool digit(char c) { return c >= '0' && c <= '9'; }
static bool letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static bool parse_version(const char *text, version *out)
{
    if (text == NULL) return false;
    size_t n = 0;
    while (n < TV4_BOOT_VERSION_BYTES && text[n] != '\0') ++n;
    if (n == 0 || n == TV4_BOOT_VERSION_BYTES) return false;
    const char *p = text, *end = text + n;
    version v = {.end = end};
    for (unsigned i = 0; i < 3; ++i) {
        if (p == end || !digit(*p)) return false;
        const char *start = p;
        uint32_t number = 0;
        while (p != end && digit(*p)) {
            uint32_t d = (uint32_t)(*p++ - '0');
            if (number > (UINT32_MAX - d) / 10u) return false;
            number = number * 10u + d;
        }
        if (p - start > 1 && *start == '0') return false;
        v.core[i] = number;
        if (i < 2 && (p == end || *p++ != '.')) return false;
    }
    v.pre = end;
    if (p != end) {
        if (*p++ != '-' || p == end) return false;
        v.pre = p;
        while (p != end) {
            const char *start = p;
            bool numeric = true;
            while (p != end && *p != '.') {
                if (!digit(*p)) { if (!letter(*p) && *p != '-') return false; numeric = false; }
                ++p;
            }
            if (p == start || (numeric && p - start > 1 && *start == '0')) return false;
            if (p != end && ++p == end) return false;
        }
    }
    *out = v;
    return true;
}
static int identifier_order(const char *a, size_t an, const char *b, size_t bn)
{
    bool ad = true, bd = true;
    for (size_t i = 0; i < an; ++i) if (!digit(a[i])) ad = false;
    for (size_t i = 0; i < bn; ++i) if (!digit(b[i])) bd = false;
    if (ad != bd) return ad ? -1 : 1;
    if (ad && an != bn) return an < bn ? -1 : 1;
    size_t n = an < bn ? an : bn;
    int c = memcmp(a, b, n);
    if (c != 0) return c < 0 ? -1 : 1;
    return (an > bn) - (an < bn);
}
bool tv4_boot_compare_versions(const char *left, const char *right, int *order)
{
    version a, b;
    if (order == NULL || !parse_version(left, &a) || !parse_version(right, &b)) return false;
    for (unsigned i = 0; i < 3; ++i) {
        if (a.core[i] != b.core[i]) { *order = a.core[i] < b.core[i] ? -1 : 1; return true; }
    }
    bool stable_a = a.pre == a.end, stable_b = b.pre == b.end;
    if (stable_a || stable_b) { *order = (int)stable_a - (int)stable_b; return true; }
    const char *ap = a.pre, *bp = b.pre;
    while (ap != a.end && bp != b.end) {
        const char *ae = ap, *be = bp;
        while (ae != a.end && *ae != '.') ++ae;
        while (be != b.end && *be != '.') ++be;
        int c = identifier_order(ap, (size_t)(ae - ap), bp, (size_t)(be - bp));
        if (c != 0) { *order = c; return true; }
        ap = ae == a.end ? ae : ae + 1;
        bp = be == b.end ? be : be + 1;
    }
    *order = (ap != a.end) - (bp != b.end);
    return true;
}
static bool valid_version(const char *text)
{
    version v;
    return parse_version(text, &v);
}
static bool valid_channel(const char channel[TV4_BOOT_CHANNEL_BYTES])
{
    return memcmp(channel, "stable", 7) == 0 || memcmp(channel, "prerelease", 11) == 0;
}
static bool valid_commit(const char commit[TV4_BOOT_COMMIT_BYTES])
{
    bool nonzero = false;
    for (size_t i = 0; i < 40; ++i) {
        if (!digit(commit[i]) && !(commit[i] >= 'a' && commit[i] <= 'f')) return false;
        if (commit[i] != '0') nonzero = true;
    }
    return commit[40] == '\0' && nonzero;
}
static bool prerelease(const char *text)
{
    version v;
    return parse_version(text, &v) && v.pre != v.end;
}
bool tv4_boot_release_fields_valid(const tv4_boot_release_candidate *c)
{
    return c != NULL && valid_version(c->release_version) && valid_version(c->component_version) &&
        valid_channel(c->channel) && valid_commit(c->source_commit);
}
bool tv4_boot_release_policy_valid(const tv4_boot_release_policy *p)
{
    if (p == NULL || !valid_version(p->installed_component_version) ||
        !valid_version(p->minimum_component_version) || !valid_version(p->selected_release_version) ||
        !valid_channel(p->channel) || !valid_commit(p->selected_source_commit)) return false;
    return prerelease(p->selected_release_version) == (strcmp(p->channel, "prerelease") == 0);
}
tv4_boot_result tv4_boot_release_compatible(const tv4_boot_release_policy *p,
    const tv4_boot_release_candidate *c)
{
    if (strcmp(c->release_version, p->selected_release_version) != 0 ||
        strcmp(c->source_commit, p->selected_source_commit) != 0 || strcmp(c->channel, p->channel) != 0 ||
        prerelease(c->release_version) != (strcmp(c->channel, "prerelease") == 0) ||
        (strcmp(c->channel, "stable") == 0 && prerelease(c->component_version))) return TV4_BOOT_COMPATIBILITY;
    int order;
    if (!tv4_boot_compare_versions(c->component_version, p->installed_component_version, &order) || order <= 0)
        return TV4_BOOT_ROLLBACK;
    if (!tv4_boot_compare_versions(c->component_version, p->minimum_component_version, &order) || order < 0)
        return TV4_BOOT_ROLLBACK;
    return TV4_BOOT_VERIFIED_CANDIDATE;
}
