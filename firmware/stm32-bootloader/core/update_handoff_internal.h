#ifndef TV4_BOOT_HANDOFF_INTERNAL_H
#define TV4_BOOT_HANDOFF_INTERNAL_H
#include "release_admission.h"
#include "update_handoff.h"
#include "release_handoff.h"
/* Private shared engine. Mode is fixed by a LOCAL typed entry point, never
 * chosen from downloaded metadata. Public legacy entries always pass false.
 * Guards/lifetime/source ownership stay in update_handoff.c. */
bool tv4_handoff_can_begin(const tv4_boot_handoff *s, const tv4_boot_handoff_config *c, const tv4_update_rx *rx);
bool tv4_handoff_begin_mode(tv4_boot_handoff *s, const tv4_boot_handoff_config *c, tv4_update_rx *rx, bool release_document);
bool tv4_handoff_verify_mode(tv4_boot_handoff *s, tv4_boot_workspace *w,
    const tv4_boot_release_policy *release_policy, tv4_boot_release_candidate *release_candidate);
bool tv4_handoff_peek_mode(tv4_boot_handoff *s, tv4_boot_handoff_receipt *out, bool release_document);
tv4_boot_image_result tv4_handoff_preflight_mode(tv4_boot_handoff *s, const tv4_boot_image_profile *profile,
    tv4_boot_workspace *w, tv4_boot_handoff_image_report *out, bool release_document);
bool tv4_handoff_clear_mode(tv4_boot_handoff *s, bool release_document);
void tv4_release_handoff_refresh(tv4_boot_release_handoff *adapter);
#endif
