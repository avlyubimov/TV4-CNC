#include "ota_service_sdk_fake.h"
#include "sdkconfig.h"
#include "ota_local_context.h"
#include "service.h"
#include "memory_diagnostics.h"
#include "memory.h"
#include "sntp_source.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

static jmp_buf escape;
static const char *mode;
static TaskFunction_t entry;
static unsigned requested,creates,locks,stack_reads,restarts,wdt_resets,wdt_deletes;
static unsigned window_starts,window_ends,startup_reports,window_reports,sends,wifi_polls;
static unsigned probes,finishes,tls_probes,acquires,releases,sntps,sessions,summaries;
static unsigned auto_attempted,auto_passed;
static unsigned receives,install_policy_logs,clock_logs;
static bool queued;static tv4_update_command queued_command;
static tv4_ota_memory_window active_window;
static tv4_ota_session session;
static uint32_t clock_ms;
static bool running,start_result,finish_passed,finish_result;
static void *probe;
static char boot_log[256];
uint8_t service_public_key[91];

static bool is(const char *s) {return !strcmp(mode,s);}
static bool boot_ok(void) {return !is("task-fail") && !is("pending-fail") && !is("probe-fail") && !is("health-timeout");}
static bool ota_running(void) {return !strncmp(mode,"ota-",4);}
static bool time_failure(void) {return !strncmp(mode,"operator-time-",14);}
static bool late_install(void) {return is("operator-time-install-after");}
static unsigned time_operations(void) {return late_install()?3u:2u;}
static bool time_range(void) {return is("operator-time-before") || is("operator-time-after") || late_install();}
static bool operator_mode(void) {return is("operator-check") || is("operator-readonly-candidate") || time_failure();}
static bool install_allowed(void) {return SERVICE_TEST_INSTALL_REVIEWED && !is("operator-readonly-candidate");}
static bool local_ready(void) {
    return SERVICE_TEST_VALID_TRUST && !SERVICE_TEST_INVALID_POLICY &&
        !is("ota-invalid-key") && !is("ota-version-mismatch") && !is("ota-no-description") &&
        !is("ota-valid-unconfigured") && !is("factory-invalid-key") && !is("factory-version-mismatch");
}
static bool finish_expected(void) {
    return !is("missing-partition") && (!ota_running() || is("ota-valid-unconfigured") || (boot_ok() && local_ready()));
}
static bool attempts(void) {return (SERVICE_TEST_SIGNED_BENCH || operator_mode()) && local_ready() && boot_ok() && finish_expected() && !is("offline") && !is("wait-health");}
void service_test_lock(void) {assert(!locks);++locks;}
void service_test_unlock(void) {assert(locks==1);--locks;}
QueueHandle_t xQueueCreateStatic(unsigned n,unsigned size,uint8_t *bytes,StaticQueue_t *state) {
    assert(!locks && n==1 && size==sizeof(tv4_update_command) && bytes && state);return state;
}
BaseType_t xTaskCreate(TaskFunction_t fn,const char *name,uint32_t size,void *arg,UBaseType_t priority,TaskHandle_t *task) {
    assert(!locks && fn && !strcmp(name,"tv4_ota") && !arg && priority==1 && !task);
    entry=fn;requested=size;++creates;return !is("task-fail") && !is("pending-fail");
}
static void dangerous_commands_blocked(void) {
    unsigned before=sends;
    assert(!tv4_ota_service_request(TV4_UPDATE_CHECK));
    assert(!tv4_ota_service_request(TV4_UPDATE_INSTALL));
    assert(!tv4_ota_service_request(TV4_UPDATE_REBOOT));assert(sends==before);
}
static void commands_blocked(void) {dangerous_commands_blocked();assert(!tv4_ota_service_request(TV4_UPDATE_CANCEL));}
BaseType_t xQueueSend(QueueHandle_t queue,const void *value,uint32_t wait) {
    assert(operator_mode() && queue && value && locks==1 && !wait && !queued);
    queued_command=*(const tv4_update_command *)value;
    assert(queued_command==TV4_UPDATE_CHECK || (queued_command==TV4_UPDATE_INSTALL && SERVICE_TEST_INSTALL_REVIEWED));
    queued=true;++sends;return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t queue,void *out,uint32_t wait) {
    assert(!locks && running && queue && out && wait==portMAX_DELAY);
    tv4_update_snapshot state;tv4_ota_service_snapshot(&state);assert(state.enabled);
    if(operator_mode()) {
        ++receives;
        if(receives==1) {
            assert(state.state==TV4_UPDATE_IDLE && !sends);
            assert(tv4_ota_service_request(TV4_UPDATE_CHECK) && queued && queued_command==TV4_UPDATE_CHECK);
            *(tv4_update_command *)out=queued_command;queued=false;return pdTRUE;
        }
        if(late_install() && receives==2) {
            /* Candidate was authenticated while time was valid. Expiry must
             * still stop INSTALL before the session or SDK writer is entered. */
            assert(state.state==TV4_UPDATE_READY && !state.error && state.install_allowed && sessions==1);
            assert(tv4_ota_service_request(TV4_UPDATE_INSTALL) && queued && queued_command==TV4_UPDATE_INSTALL);
            *(tv4_update_command *)out=queued_command;queued=false;return pdTRUE;
        }
        if(time_failure()) {
            assert(state.state==TV4_UPDATE_ERROR && state.error==(time_range()?TV4_UPDATE_TIME_RANGE_ERROR:3));
            assert(!queued && acquires==receives-1 && releases==acquires && !active_window);
            if(!late_install())assert(!state.version[0] && !state.total && !state.received);
            assert(!tv4_ota_service_request(TV4_UPDATE_INSTALL) && !tv4_ota_service_request(TV4_UPDATE_REBOOT));
            if(receives<=time_operations()) {
                assert(tv4_ota_service_request(TV4_UPDATE_CHECK) && queued && queued_command==TV4_UPDATE_CHECK);
                *(tv4_update_command *)out=queued_command;queued=false;return pdTRUE;
            }
            assert(receives==time_operations()+1);
            longjmp(escape,1);
        }
        assert(receives==2 && state.state==TV4_UPDATE_READY && !state.error && !queued);
        assert(state.install_allowed==install_allowed());
        assert(tv4_ota_service_request(TV4_UPDATE_INSTALL)==install_allowed());
        assert(sends==1u+(unsigned)install_allowed() && queued==install_allowed());
        longjmp(escape,1);
    }
    if(!boot_ok())assert(state.state==TV4_UPDATE_BOOT_FAILED);
    else if(local_ready() && SERVICE_TEST_SIGNED_BENCH) {
        assert(summaries==1);
        if(is("success")) {assert(state.state==TV4_UPDATE_READY && !state.error && !strcmp(state.version,"0.1.0-bench.2"));}
        else {
            assert(state.state==TV4_UPDATE_ERROR && state.error);
            if(is("cancel-session"))assert(state.error==TV4_UPDATE_CANCELLED_ERROR);
            if(is("sntp-range"))assert(state.error==TV4_UPDATE_TIME_RANGE_ERROR);
        }
    }else assert(state.state==(local_ready()?TV4_UPDATE_IDLE:TV4_UPDATE_SETUP) && !state.error);
    assert(state.stack_valid && state.stack_watermark==requested-256);
    if(!local_ready() || SERVICE_TEST_SIGNED_BENCH || !boot_ok())commands_blocked();
    else {
        /* Boot admission is independent of AP/DHCP/SNTP availability. */
        assert(!wifi_polls && !sntps && !sessions);
        assert(!tv4_ota_service_request(TV4_UPDATE_INSTALL));
        assert(!tv4_ota_service_request(TV4_UPDATE_REBOOT));
    }
    longjmp(escape,1);
}
void vTaskDelay(uint32_t ticks) {
    assert(!locks && running && (ticks==50 || (SERVICE_TEST_SIGNED_BENCH && finishes && ticks==250)));
    clock_ms+=ticks;assert(clock_ms<=75000);
    tv4_ota_service_health(!is("health-timeout") && !(finishes && is("wait-health")));
}
int64_t esp_timer_get_time(void) {return (int64_t)clock_ms*1000;}
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t task) {assert(!locks && running && !task);++stack_reads;return requested-256;}
esp_err_t esp_task_wdt_add(TaskHandle_t task) {assert(!locks && running && !task);return ESP_OK;}
esp_err_t esp_task_wdt_reset(void) {assert(!locks && running);++wdt_resets;return ESP_OK;}
esp_err_t esp_task_wdt_delete(TaskHandle_t task) {assert(!locks && running && !task);++wdt_deletes;return ESP_OK;}
_Noreturn void esp_restart(void) {assert(!locks);++restarts;longjmp(escape,1);}
const esp_app_desc_t *esp_app_get_description(void) {
#if SERVICE_TEST_VALID_TRUST
    static const esp_app_desc_t desc={.version="0.1.0-bench.1"};
    static const esp_app_desc_t mismatch={.version="0.1.0-bench.2"};
    if(is("ota-no-description"))return NULL;
    if(is("ota-version-mismatch") || is("factory-version-mismatch"))return &mismatch;
#else
    static const esp_app_desc_t desc={.version="host-service-test"};
#endif
    return &desc;
}
void esp_chip_info(esp_chip_info_t *chip) {assert(SERVICE_TEST_VALID_TRUST && attempts() && chip);chip->revision=2;}
const esp_partition_t *esp_ota_get_running_partition(void) {
    static const esp_partition_t factory={.address=0x10000,.size=0x200000};
    static const esp_partition_t ota={.subtype=0x10,.address=0x220000,.size=0x300000};
    return is("missing-partition")?NULL:ota_running()?&ota:&factory;
}
esp_err_t esp_ota_get_state_partition(const esp_partition_t *part,esp_ota_img_states_t *state) {
    assert(part && state);*state=is("pending-fail") || (ota_running() && !is("ota-valid-unconfigured"))?ESP_OTA_IMG_PENDING_VERIFY:ESP_OTA_IMG_VALID;return ESP_OK;
}
bool __wrap_tv4_ota_sdk_layout_valid(void) {assert(!locks && running);return true;}
bool __wrap_tv4_ota_sdk_finish_boot(bool passed) {
    assert(!locks && running);++finishes;finish_passed=passed;
    /* Same boundary contract as installer_sdk.c: factory/previously VALID
     * stay recoverable. A PENDING_VERIFY image may only be confirmed on true. */
    const esp_partition_t *p=esp_ota_get_running_partition();
    finish_result=p && (p->subtype==ESP_PARTITION_SUBTYPE_APP_FACTORY || is("ota-valid-unconfigured") || passed);
    return finish_result;
}
void *heap_caps_malloc(size_t bytes,unsigned caps) {
    assert(!locks && running && bytes==256 && caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));++probes;
    probe=is("probe-fail")?NULL:malloc(bytes);return probe;
}
void heap_caps_free(void *p) {assert(!locks && p==probe);free(p);probe=NULL;}
bool esp_ptr_external_ram(const void *p) {assert(!locks);return p && p==probe;}
size_t heap_caps_get_free_size(unsigned caps) {(void)caps;return 100000;}
bool wifi_service_online(void) {
    assert((SERVICE_TEST_SIGNED_BENCH || operator_mode()) && SERVICE_TEST_VALID_TRUST && running && finishes && finish_passed && !locks);
    ++wifi_polls;return !is("offline");
}
bool tv4_utc_tls_bench_run_in_ota_worker(bool (*cancelled)(void *),void *context) {
    assert(SERVICE_TEST_TLS_BENCH && running && !locks && !context && cancelled);
    assert(finishes==1 && finish_passed && wdt_deletes==1 && window_ends==1 && window_reports==1);
    commands_blocked();tv4_update_snapshot state;tv4_ota_service_snapshot(&state);
    assert(state.state==TV4_UPDATE_SETUP && !cancelled(context));
    tv4_ota_service_health(false);assert(cancelled(context));
    tv4_ota_service_health(true);assert(!cancelled(context));++tls_probes;return false;
}
bool tv4_ota_memory_window_begin(tv4_ota_memory_window id) {
    assert(!locks && !active_window);
    if(id==TV4_MEM_BOOT)assert(!running);
    else assert((SERVICE_TEST_SIGNED_BENCH || operator_mode()) && running && finishes && wdt_deletes==1);
    active_window=id;++window_starts;return true;
}
bool tv4_ota_memory_window_end(tv4_ota_memory_window id) {assert(!locks && active_window==id);active_window=0;++window_ends;return true;}
void tv4_ota_memory_report_startup(void) {assert(!locks && running);++startup_reports;}
void tv4_ota_memory_report_window(void) {assert(!locks && running && !active_window);++window_reports;}
tv4_ota_memory_result __wrap_tv4_ota_memory_acquire_psram(tv4_ota_memory *lease,const tv4_ota_memory_policy *policy) {
    assert(attempts() && lease && !lease->session && policy->reviewed && policy->internal_free_floor==65536 && active_window==TV4_MEM_WORKSPACE);
    assert(acquires++<(time_failure()?time_operations():1u));dangerous_commands_blocked();if(is("memory-fail"))return TV4_MEMORY_HEADROOM;
    lease->session=&session;return TV4_MEMORY_OK;
}
tv4_ota_memory_result __wrap_tv4_ota_memory_release(tv4_ota_memory *lease) {
    assert(attempts() && !locks && active_window==TV4_MEM_CLEANUP && lease && releases++<(time_failure()?time_operations():1u));
    memset(lease,0,sizeof(*lease));memset(&session,0,sizeof(session));return TV4_MEMORY_OK;
}
tv4_sntp_result __wrap_tv4_sntp_sync(tv4_time_anchor *anchor,const tv4_time_policy *policy,uint32_t timeout,
    bool (*cancelled)(void *),void *context,tv4_sntp_report *report) {
    assert(attempts() && !locks && active_window==TV4_MEM_SNTP && timeout==10000 && sntps++<(time_failure()?time_operations():1u) && !cancelled(context));
    dangerous_commands_blocked();memset(report,0,sizeof(*report));report->cleanup_ok=!is("sntp-cleanup-fail");
    if(is("sntp-fail"))return TV4_SNTP_TIMEOUT;
    if(is("sntp-range")) {report->time_result=TV4_TIME_RANGE;return TV4_SNTP_TIME_REJECTED;}
    if(time_failure() && (!late_install() || sntps>1)) {
        if(is("operator-time-timeout"))return TV4_SNTP_TIMEOUT;
        if(is("operator-time-cleanup") || is("operator-time-cleanup-range")) {
            report->cleanup_ok=false;report->time_result=is("operator-time-cleanup-range")?TV4_TIME_RANGE:TV4_TIME_OK;
            return TV4_SNTP_CLEANUP;
        }
        if(is("operator-time-source")) {report->time_result=TV4_TIME_SOURCE;return TV4_SNTP_TIME_REJECTED;}
        tv4_time_sample refused={-1,(uint64_t)clock_ms*1000,TV4_TIME_SNTP_UNAUTHENTICATED};
        if(is("operator-time-before"))refused.epoch_us=((int64_t)policy->earliest_epoch-1)*1000000;
        if(is("operator-time-after") || late_install())refused.epoch_us=((int64_t)policy->latest_epoch+1)*1000000;
        report->time_result=tv4_time_adopt(anchor,policy,&refused);
        assert(report->time_result==(time_range()?TV4_TIME_RANGE:TV4_TIME_INVALID_CLOCK) && !anchor->ready);
        return TV4_SNTP_TIME_REJECTED;
    }
    const tv4_time_sample sample={1800000000000000LL,(uint64_t)clock_ms*1000,TV4_TIME_SNTP_UNAUTHENTICATED};
    assert(tv4_time_adopt(anchor,policy,&sample)==TV4_TIME_OK);return TV4_SNTP_OK;
}
bool __wrap_tv4_ota_session_run_guarded(tv4_ota_session *s,const tv4_session_request *r,const tv4_session_io *io,
    const tv4_ota_resource_policy *policy,tv4_ota_resource_reader reader,void *context,tv4_ota_resource_report *report) {
    assert(attempts() && !locks && active_window==TV4_MEM_SESSION && s==&session && !sessions++);
    assert(r->verify_image==(bool)SERVICE_TEST_SIGNED_BENCH && r->trusted_time && r->total_timeout_ms==300000 && r->io_timeout_ms==5000 && r->maximum_redirects==3);
    assert(policy->reviewed && policy->stack_floor==3072 && reader && !context && io->fetch && !io->cancelled(io->context));
    dangerous_commands_blocked();
    *report=(tv4_ota_resource_report){TV4_RESOURCE_OK,2,{100000,64000,5000000,4000000,8000}};
    if(is("cancel-session")) {assert(tv4_ota_service_request(TV4_UPDATE_CANCEL));assert(io->cancelled(io->context));}
    bool ok=!is("session-fail") && !is("cancel-session") && !is("resource-fail");
    s->result.state=ok?(operator_mode()?TV4_SESSION_CANDIDATE_READY:TV4_SESSION_IMAGE_VERIFIED):TV4_SESSION_FAILED;
    s->result.error=ok?TV4_SESSION_NONE:is("resource-fail")?TV4_SESSION_RESOURCE:is("cancel-session")?TV4_SESSION_CANCEL:TV4_SESSION_TRANSPORT;
    if(is("resource-fail"))report->result=TV4_RESOURCE_STACK;
    if(ok) {strcpy(s->candidate.version,"0.1.0-bench.2");s->candidate.size_bytes=1024;s->candidate.installable=!is("operator-readonly-candidate");}
    return ok;
}
bool __wrap_tv4_ota_session_install_guarded(tv4_ota_session *s,const tv4_session_request *r,const tv4_session_io *io,
    const tv4_ota_resource_policy *p,tv4_ota_resource_reader reader,void *c,tv4_ota_resource_report *report,const tv4_session_sink *sink) {
    (void)s;(void)r;(void)io;(void)p;(void)reader;(void)c;(void)report;(void)sink;assert(!"read-only bench entered installer");return false;
}
void service_test_log(const char *tag,const char *format,...) {
    assert(!locks && running && !strcmp(tag,"tv4_ota"));
    char message[256];va_list args;va_start(args,format);int n=vsnprintf(message,sizeof(message),format,args);va_end(args);
    assert(n>0 && (size_t)n<sizeof(boot_log));
    if(strstr(message,"boot_self_test"))memcpy(boot_log,message,(size_t)n+1);
    else if(strstr(message,"install_policy=")) {
        assert(!install_policy_logs++);
        assert(!strcmp(message,SERVICE_TEST_INSTALL_REVIEWED?"install_policy=owner_reviewed":"install_policy=check_only"));
    }
    else if(strstr(message,"signed_check_bench")) {
        assert(SERVICE_TEST_SIGNED_BENCH && !summaries++);
        assert(sscanf(message,"signed_check_bench attempted=%u pass=%u read_only=1",&auto_attempted,&auto_passed)==2);
    }else if(strstr(message,"clock result=")) {
        unsigned result,time,cleanup;char extra;
        assert(attempts() && ++clock_logs==sntps);
        assert(sscanf(message,"clock result=%u time=%u cleanup=%u%c",&result,&time,&cleanup,&extra)==3);
        if(time_range() && (!late_install() || clock_logs>1))assert(result==TV4_SNTP_TIME_REJECTED && time==TV4_TIME_RANGE && cleanup==1);
        if(is("sntp-range"))assert(result==TV4_SNTP_TIME_REJECTED && time==TV4_TIME_RANGE && cleanup==1);
        if(late_install() && clock_logs==1)assert(result==TV4_SNTP_OK && time==TV4_TIME_OK && cleanup==1);
        if(is("operator-time-timeout"))assert(result==TV4_SNTP_TIMEOUT && time==TV4_TIME_OK && cleanup==1);
        if(is("operator-time-clock"))assert(result==TV4_SNTP_TIME_REJECTED && time==TV4_TIME_INVALID_CLOCK && cleanup==1);
        if(is("operator-time-source"))assert(result==TV4_SNTP_TIME_REJECTED && time==TV4_TIME_SOURCE && cleanup==1);
        if(is("operator-time-cleanup") || is("operator-time-cleanup-range"))assert(result==TV4_SNTP_CLEANUP && cleanup==0);
    }else if(strstr(message,"stage=") || strstr(message,"session state="))assert(attempts());
    else assert(SERVICE_TEST_TLS_BENCH && tls_probes==1 && strstr(message,"worker_tls_bench pass=0"));
}
int main(int argc,char **argv) {
    assert(argc==2);mode=argv[1];
    if(SERVICE_TEST_VALID_TRUST) {
        EVP_PKEY *key=EVP_PKEY_Q_keygen(NULL,NULL,"EC","prime256v1");assert(key);
        unsigned char *der=NULL;int length=i2d_PUBKEY(key,&der);assert(length==91);
        memcpy(service_public_key,der,91);OPENSSL_free(der);EVP_PKEY_free(key);
        if(is("ota-invalid-key") || is("ota-valid-unconfigured") || is("factory-invalid-key"))memset(service_public_key,0,91);
    }
    if(!setjmp(escape)) {
        start_result=tv4_ota_service_start();assert(!tv4_ota_service_start());
        tv4_update_snapshot state;tv4_ota_service_snapshot(&state);assert(!state.stack_valid && !stack_reads);
        tv4_ota_service_health(true);commands_blocked();
        if(is("task-fail"))assert(!start_result && state.state==TV4_UPDATE_SETUP && state.error==21);
        else {assert(start_result && entry);running=true;entry(NULL);assert(!"worker returned");}
    }
    assert(creates==1 && requested==(SERVICE_TEST_PROVISIONED?12288u:SERVICE_TEST_BENCH?8192u:3072u));
    assert(!locks && sends==(time_failure()?time_operations():operator_mode()?1u+(unsigned)install_allowed():0u) && !probe && !active_window && window_starts==window_ends);
    assert(tls_probes==(unsigned)(SERVICE_TEST_TLS_BENCH && is("success")));
    assert(acquires==(time_failure()?time_operations():(unsigned)attempts()) && releases==acquires);
    assert(sntps==(time_failure()?time_operations():(unsigned)(attempts() && !is("memory-fail"))) && clock_logs==sntps);
    assert(sessions==(late_install()?1u:(unsigned)(sntps && !time_failure() && !is("sntp-range") && !is("sntp-fail") && !is("sntp-cleanup-fail"))));
    assert((wifi_polls>0)==((SERVICE_TEST_SIGNED_BENCH || operator_mode()) && SERVICE_TEST_VALID_TRUST && boot_ok()));
    if(is("task-fail") || is("pending-fail")) {
        assert(!stack_reads && !probes && !finishes && !startup_reports && !window_reports && window_starts==1);
        assert(restarts==(unsigned)is("pending-fail"));
    }else {
        bool success=boot_ok();
        assert(finishes==1 && finish_passed==(success && !is("missing-partition") && (!ota_running() || local_ready())));
        assert(finish_result==finish_expected());
        assert(startup_reports==1 && window_reports==window_ends && stack_reads>0 && wdt_resets>0);
        assert(wdt_deletes==(unsigned)finish_expected() && restarts==(unsigned)!finish_expected());
        assert(probes==(unsigned)!is("health-timeout"));if(is("health-timeout"))assert(clock_ms==30000);
        if(SERVICE_TEST_PROVISIONED)assert(strstr(boot_log,"worker_stack_bytes=12288 provisioned=1"));
        else if(SERVICE_TEST_BENCH)assert(strstr(boot_log,"worker_stack_bytes=8192 stack_bench=1 provisioned=0"));
        else assert(!strstr(boot_log,"stack_bench="));
    }
    assert(install_policy_logs==(unsigned)(!is("task-fail") && !is("pending-fail") && finish_expected()));
    assert(summaries==(unsigned)(SERVICE_TEST_SIGNED_BENCH && boot_ok() && finish_expected()));
    assert(auto_attempted==(unsigned)(SERVICE_TEST_SIGNED_BENCH && attempts()));assert(auto_passed==(unsigned)(SERVICE_TEST_SIGNED_BENCH && attempts() && is("success")));
    if(SERVICE_TEST_VALID_TRUST && (is("offline") || is("wait-health")))assert(clock_ms>=45000 && clock_ms<=50000);
    printf("actual ESP service requested=%u mode=%s read-only=%u attempts=%u PASS\n",requested,mode,(unsigned)SERVICE_TEST_SIGNED_BENCH,auto_attempted);
    return 0;
}
