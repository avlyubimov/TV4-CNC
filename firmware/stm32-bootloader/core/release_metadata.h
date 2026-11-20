#ifndef TV4_BOOT_RELEASE_METADATA_H
#define TV4_BOOT_RELEASE_METADATA_H
#include "metadata.h"
#include "release_admission.h"
/* Private pure policy/parser helpers. Additional parser fields are scratch
 * until the whole operation succeeds; no public candidate is published here. */
bool tv4_boot_compare_versions(const char *left, const char *right, int *order);
bool tv4_boot_release_policy_valid(const tv4_boot_release_policy *policy);
bool tv4_boot_release_fields_valid(const tv4_boot_release_candidate *candidate);
bool tv4_boot_parse_release_metadata(const uint8_t *bytes, size_t length,
    tv4_boot_metadata *metadata, tv4_boot_release_candidate *extra);
tv4_boot_result tv4_boot_release_compatible(const tv4_boot_release_policy *policy,
    const tv4_boot_release_candidate *candidate);
#endif
