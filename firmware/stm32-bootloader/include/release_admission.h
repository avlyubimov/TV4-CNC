#ifndef TV4_BOOT_RELEASE_ADMISSION_H
#define TV4_BOOT_RELEASE_ADMISSION_H
#include "admission.h"

#define TV4_BOOT_RELEASE_SCHEMA "tv4-motion-release-draft-1"
#define TV4_BOOT_VERSION_BYTES 32u
#define TV4_BOOT_COMMIT_BYTES 41u
#define TV4_BOOT_CHANNEL_BYTES 12u

/* Explicit opt-in EXPERIMENTAL signed Motion release document, not the HMI
 * manifest, a production bundle/CAN ABI, installed-state provider or installer.
 * All context is local/reviewed, not populated from the downloaded document.
 * SemVer subset: 1..31 ASCII bytes, uint32 core, prerelease, no build metadata.
 * Numeric image/boot/protocol counters remain independent of SemVer.
 * source_commit selects an exact lowercase40hex nonzero signer claim; it is
 * not evidence that the signer built the supplied image from that commit. */
typedef struct {
    tv4_boot_policy image;
    char installed_component_version[TV4_BOOT_VERSION_BYTES];
    char minimum_component_version[TV4_BOOT_VERSION_BYTES];
    char selected_release_version[TV4_BOOT_VERSION_BYTES];
    char selected_source_commit[TV4_BOOT_COMMIT_BYTES];
    char channel[TV4_BOOT_CHANNEL_BYTES]; /* stable or prerelease, never unknown */
} tv4_boot_release_policy;
typedef struct {
    tv4_boot_candidate image; /* metadata SHA covers this new raw document */
    char release_version[TV4_BOOT_VERSION_BYTES];
    char component_version[TV4_BOOT_VERSION_BYTES];
    char source_commit[TV4_BOOT_COMMIT_BYTES];
    char channel[TV4_BOOT_CHANNEL_BYTES];
} tv4_boot_release_candidate;

/* Same bounded workspace/crypto owner/request callbacks/deadline/EOF contract
 * as tv4_boot_admit, with a SEPARATE strict15-field schema. Original draft/API
 * does not accept it. No fallback, double verification or additional arena.
 * Inputs snapshotted before callbacks. Return candidate only after signature,
 * release selection/version/channel, target/counter, full image SHA/EOF/cleanup.
 * Valid disjoint typed objects required. Failure clears output except invalid
 * aliases/BUSY (untouched). There is no live lease/generation/vector check here;
 * this standalone result is historical, not permission to install/ARM/jump.
 * release_handoff.h provides an opt-in live coordinator; no runtime caller. */
tv4_boot_result tv4_boot_admit_release(const tv4_boot_release_policy *policy,
    const tv4_boot_request *request, tv4_boot_workspace *workspace,
    tv4_boot_release_candidate *candidate);
#endif
