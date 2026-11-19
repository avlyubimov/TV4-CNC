#ifndef TV4_MOTION_NOIO_H
#define TV4_MOTION_NOIO_H
#include "supervisor.h"
/* No MMIO, pin assignments, clock changes, watchdog or physical safety feedback.
 * Inhibit returns false; watchdog is unavailable. Supervisor must remain faulted.
 * This incomplete adapter is not evidence that physical outputs are inhibited. */
tv4_motion_platform tv4_motion_noio_platform(void);
#endif
