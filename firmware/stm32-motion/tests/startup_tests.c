#include "memory_init.h"
#include "noio.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static void memory_tests(void)
{
    const uint32_t load[] = {11, 22, 33};
    uint32_t data[] = {0xface, 1, 2, 3, 0xbeef};
    uint32_t bss[] = {0xabcd, 4, 5, 6, 0xdcba};
    assert(tv4_startup_memory_init(data + 1, load, 3, bss + 1, 3));
    assert(!memcmp(data + 1, load, sizeof(load)));
    assert(data[0] == 0xface && data[4] == 0xbeef);
    assert(bss[0] == 0xabcd && bss[1] == 0 && bss[2] == 0 && bss[3] == 0 && bss[4] == 0xdcba);
    assert(load[0] == 11 && load[1] == 22 && load[2] == 33);
    assert(tv4_startup_memory_init(NULL, NULL, 0, NULL, 0));
    assert(tv4_startup_memory_init(data + 1, data + 1, 3, NULL, 0));
    uint32_t before[5];
    memcpy(before, data, sizeof(data));
    assert(!tv4_startup_memory_init(data, load, 3, NULL, 1));
    assert(!tv4_startup_memory_init(data, NULL, 3, bss, 1));
    assert(!tv4_startup_memory_init(NULL, load, 3, bss, 1));
    assert(!tv4_startup_memory_init((unsigned char *)data + 1, load, 1, bss, 1));
    assert(!tv4_startup_memory_init(data, (const unsigned char *)load + 1, 1, bss, 1));
    assert(!tv4_startup_memory_init(data, load, 1, (unsigned char *)bss + 1, 1));
    assert(!tv4_startup_memory_init(data, load, SIZE_MAX, bss, 1));
    assert(!tv4_startup_memory_init(data, load, 1, bss, SIZE_MAX));
    assert(!tv4_startup_memory_init(data, load, SIZE_MAX / sizeof(uint32_t), bss, 1));
    assert(!tv4_startup_memory_init(data + 1, data, 3, bss, 1));
    assert(!tv4_startup_memory_init(data, load, 3, data + 1, 1));
    assert(!tv4_startup_memory_init(data, load, 3, (void *)load, 1));
    assert(!memcmp(before, data, sizeof(data))); /* Every failed init is write-free. */
}
static void noio_tests(void)
{
    tv4_motion_platform p = tv4_motion_noio_platform();
    assert(p.inhibit_outputs && !p.inhibit_outputs(p.context) && !p.feed_watchdog);
    tv4_motion_supervisor m;
    tv4_motion_init(&m, (tv4_motion_config){100, 10}, p, 0, false);
    assert(!m.configured && m.state == TV4_MOTION_FAULT);
    assert((m.latched_faults & (TV4_MOTION_PLATFORM | TV4_MOTION_CONFIG)) ==
                              (TV4_MOTION_PLATFORM | TV4_MOTION_CONFIG));
    tv4_motion_self_test(&m, true);
    tv4_motion_session(&m, 1);
    assert(!tv4_motion_heartbeat(&m, 1, 1));
    for (uint32_t t = 1; t < 1000; ++t) {
        tv4_motion_cycle(&m, t, (tv4_motion_inputs){true, false});
        assert(m.state == TV4_MOTION_FAULT && !tv4_motion_ack_faults(&m, t));
    }
}
int main(void)
{
    memory_tests();
    noio_tests();
    return 0;
}
