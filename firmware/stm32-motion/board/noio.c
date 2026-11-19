#include "noio.h"
#include <stddef.h>
static bool unavailable_inhibit(void *context)
{
    (void)context;
    return false;
}
tv4_motion_platform tv4_motion_noio_platform(void)
{
    return (tv4_motion_platform){.context = NULL, .inhibit_outputs = unavailable_inhibit,
                                 .feed_watchdog = NULL};
}
