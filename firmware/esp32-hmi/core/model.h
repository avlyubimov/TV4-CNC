#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "journal.h"
#include "tools.h"
typedef enum { SIMULATION, DISCONNECTED, READY, HOLD, FAULT } hmi_mode_t;
typedef enum { EV_STEP, EV_STOP, EV_DISCONNECT, EV_CONNECT, EV_EMERGENCY, EV_ACK, EV_X_MINUS, EV_Z_MINUS, EV_Z_PLUS, EV_INC_1, EV_INC_10, EV_INC_100, EV_INC_1000, EV_TOUCH_FAULT, EV_DISPLAY_FAULT, EV_QUEUE_FAULT, EV_INVALID_FAULT, EV_NEW_SIM, EV_RPM_MINUS, EV_RPM_PLUS } event_t;
typedef struct {
    hmi_mode_t mode;
    fault_t fault;
    journal_t journal;
    tools_t tools;
    bool connected, emergency, heartbeat_seen;
    uint32_t heartbeat_ms, clock_ms;
    uint8_t sequence;
    int32_t x_um, z_um, step_um;
    uint16_t rpm, feed, tool, feed_override;
    uint32_t rejected;
} model_t;
void model_init(model_t *m);
void model_event(model_t *m, event_t e);
void model_tick(model_t *m, uint32_t now);
bool model_receive(model_t *m, const uint8_t *data, unsigned len, uint32_t now);
/* Transport stub: deliberately cannot transmit any command. */
bool transport_send(const void *data, unsigned len);

/* Simulation library only: never applies offsets or transmits tool commands. */
bool model_save_tool(model_t *model,unsigned index,const tool_geometry_t *geometry);
bool model_activate_tool(model_t *model,unsigned index);

/* Simulation-only feed override, percent: 0..150; rejects non-simulation states. */
bool model_set_feed_override(model_t *model,unsigned percent);
