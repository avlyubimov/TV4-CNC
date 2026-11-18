#include "model.h"
#include <string.h>
void model_init(model_t *m) {
    memset(m, 0, sizeof(*m)); m->mode=SIMULATION;
    tools_init(&m->tools);
    journal_init(&m->journal); journal_push(&m->journal,0,J_BOOT,F_NONE);
    m->feed_override=100; m->step_um=100; m->x_um=12500; m->z_um=-42500; m->tool=1;
}
static void fault(model_t *m,fault_t cause) {
    if(m->mode==FAULT && m->fault==cause) return;
    m->fault=cause; m->mode=FAULT; m->rpm=m->feed=0;
    m->emergency=cause==F_EMERGENCY;
    journal_push(&m->journal,m->clock_ms,J_FAULT,cause);
}
void model_event(model_t *m, event_t e) {
    journal_kind_t kind=J_REJECTED;
    switch(e) {
    case EV_EMERGENCY: fault(m,F_EMERGENCY); return;
    case EV_TOUCH_FAULT: fault(m,F_TOUCH); return;
    case EV_DISPLAY_FAULT: fault(m,F_DISPLAY); return;
    case EV_QUEUE_FAULT: fault(m,F_QUEUE); return;
    case EV_INVALID_FAULT: fault(m,F_INVALID); return;
    case EV_NEW_SIM:
        /* Explicit reset of this simulation provider, not machine authorization. */
        if(m->mode!=HOLD || m->fault!=F_NONE) { m->rejected++; journal_push(&m->journal,m->clock_ms,J_REJECTED,m->fault); return; }
        m->mode=SIMULATION; m->connected=m->heartbeat_seen=false;
        m->work_x_um=m->work_z_um=0; m->work_frame=false;
        m->x_um=25430; m->z_um=-142850; m->rpm=1250; m->feed=(uint16_t)(180u*m->feed_override/100u);
        journal_push(&m->journal,m->clock_ms,J_RESTART,F_NONE); return;
    case EV_STOP: if(m->mode!=FAULT) m->mode=HOLD; kind=J_STOP; break;
    case EV_DISCONNECT: m->connected=false; m->heartbeat_seen=false;
        if(m->mode!=FAULT) m->mode=DISCONNECTED;
        kind=J_DISCONNECT; break;
    case EV_CONNECT: /* Virtual reconnect never authorizes operation. */
        m->connected=true; m->heartbeat_seen=false; m->heartbeat_ms=m->clock_ms;
        if(m->mode!=FAULT) m->mode=HOLD;
        kind=J_CONNECT; break;
    case EV_ACK:
        journal_push(&m->journal,m->clock_ms,J_ACK,m->fault);
        m->emergency=false; m->fault=F_NONE; m->mode=HOLD; m->rpm=m->feed=0; return;
    case EV_FRAME_TOGGLE: m->work_frame=!m->work_frame; kind=J_FRAME; break;
    case EV_ZERO_WORK_X:
    case EV_ZERO_WORK_Z:
    case EV_ZERO_WORK_ALL:
        if(m->mode==SIMULATION && !m->connected) {
            if(e!=EV_ZERO_WORK_Z)m->work_x_um=m->x_um;
            if(e!=EV_ZERO_WORK_X)m->work_z_um=m->z_um;
            m->work_frame=true;
            kind=e==EV_ZERO_WORK_X?J_ZERO_X:e==EV_ZERO_WORK_Z?J_ZERO_Z:J_ZERO_ALL;
        }
        break;
    case EV_RPM_MINUS:
        if(m->mode==SIMULATION && !m->connected && m->rpm>=50) { m->rpm-=50; kind=J_RPM; } break;
    case EV_RPM_PLUS:
        if(m->mode==SIMULATION && !m->connected && m->rpm<=2950) { m->rpm+=50; kind=J_RPM; } break;
    case EV_STEP: if(m->mode==SIMULATION && m->x_um<=1000000-m->step_um) { m->x_um+=m->step_um; kind=J_STEP; } break;
    case EV_X_MINUS: if(m->mode==SIMULATION && m->x_um>=-1000000+m->step_um) { m->x_um-=m->step_um; kind=J_STEP; } break;
    case EV_Z_MINUS: if(m->mode==SIMULATION && m->z_um>=-1000000+m->step_um) { m->z_um-=m->step_um; kind=J_STEP; } break;
    case EV_Z_PLUS: if(m->mode==SIMULATION && m->z_um<=1000000-m->step_um) { m->z_um+=m->step_um; kind=J_STEP; } break;
    case EV_INC_1: m->step_um=1; kind=J_INCREMENT; break;
    case EV_INC_10: m->step_um=10; kind=J_INCREMENT; break;
    case EV_INC_100: m->step_um=100; kind=J_INCREMENT; break;
    case EV_INC_1000: m->step_um=1000; kind=J_INCREMENT; break;
    default: fault(m,F_INVALID); m->rejected++; return;
    }
    if(kind==J_REJECTED) m->rejected++;
    journal_push(&m->journal,m->clock_ms,kind,m->fault);
    if(m->mode!=SIMULATION) { m->rpm=0; m->feed=0; }
}
void model_tick(model_t *m, uint32_t now) {
    m->clock_ms=now;
    if(m->connected && (uint32_t)(now-m->heartbeat_ms)>=500) {
        model_event(m, EV_DISCONNECT);
        journal_push(&m->journal,now,J_TIMEOUT,m->fault);
    }
}
bool model_receive(model_t *m, const uint8_t *d, unsigned n, uint32_t now) {
    model_tick(m,now);
    /* Bench envelope, NOT allocated CAN identifiers. No command decoder. */
    if(!m->connected || !d || n!=4 || d[0]!=1 || d[2]!=0 || d[3]!=0 ||
       (m->heartbeat_seen && (uint8_t)(d[1]-m->sequence)!=1)) {
        m->rejected++; fault(m,F_PROTOCOL); return false;
    }
    m->sequence=d[1]; m->heartbeat_ms=now; m->heartbeat_seen=true;
    return true;
}
bool transport_send(const void *data, unsigned len) {
    (void)data; (void)len; return false;
}

static bool tool_reject(model_t *m) {
    m->rejected++; journal_push(&m->journal,m->clock_ms,J_REJECTED,m->fault);return false;
}
bool model_save_tool(model_t *m,unsigned index,const tool_geometry_t *g) {
    if(m->mode==FAULT || m->connected || !tools_save(&m->tools,index,g))return tool_reject(m);
    journal_push(&m->journal,m->clock_ms,J_TOOL_SAVE,F_NONE);return true;
}
bool model_activate_tool(model_t *m,unsigned index) {
    if(m->mode==FAULT || m->connected || !tools_activate(&m->tools,index))return tool_reject(m);
    m->tool=(uint16_t)(index+1);journal_push(&m->journal,m->clock_ms,J_TOOL_SELECT,F_NONE);return true;
}

bool model_set_feed_override(model_t *m,unsigned percent) {
    if(percent>150 || m->mode!=SIMULATION || m->connected) return tool_reject(m);
    if(m->feed_override==percent) return true;
    m->feed_override=(uint16_t)percent;
    m->feed=(uint16_t)(180u*percent/100u);
    journal_push(&m->journal,m->clock_ms,J_OVERRIDE,F_NONE);
    return true;
}

int32_t model_display_x(const model_t *m) { return m->work_frame?m->x_um-m->work_x_um:m->x_um; }
int32_t model_display_z(const model_t *m) { return m->work_frame?m->z_um-m->work_z_um:m->z_um; }
