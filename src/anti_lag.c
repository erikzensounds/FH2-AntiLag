/* GPL-3.0. Experimental FH2 anti-lag; uses Extra Options' plugin API.
   Additive in-memory shop catalogue. No direct save or asset changes. */
#include "fh2_plugin.h"
#include "controller.h"
#include "native_pulse.h"
#include "effect_window.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
#include "signatures.h"
static unsigned char *image;
static const FH2PluginContext *context;
static ALSState state;
static uintptr_t last_physics;
static PVOID volatile pop_identity;
static volatile LONG enabled;
static int ready;
static float duration=6.0f,retain=0.85f,min_rpm=1800,interval=0.14f;
static HANDLE logfile=INVALID_HANDLE_VALUE;
static unsigned holds,pops;
static LARGE_INTEGER counter_frequency,last_counter;
static ULONGLONG next_diagnostic;
static unsigned diagnostic_count,guard_mask;
static void (*burble_next)(void *,float,void *);
static ALSNativePulse native_pulse;
static volatile LONG64 burst_until;
static PVOID volatile effect_car,audio_snapshot;
static volatile LONG visual_count,audio_count,snapshot_count;
static void (*snapshot_next)(void *,void *);
static void (*visual_next)(void *,void *);
static void (*audio_next)(void *,void *,void *,void *,void *,void *,void *);
static void log_line(const char *s){
    if(context && context->log_utf8)context->log_utf8(s);
    if(logfile!=INVALID_HANDLE_VALUE){DWORD n;WriteFile(logfile,s,(DWORD)strlen(s),&n,NULL);}
}
static int readable(const void *p,size_t bytes){
    MEMORY_BASIC_INFORMATION m;
    return p && VirtualQuery(p,&m,sizeof m)==sizeof m && m.State==MEM_COMMIT && !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) &&
       (uintptr_t)p+bytes>=(uintptr_t)p && (uintptr_t)p+bytes<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static float number(const void *p,unsigned off){float x;memcpy(&x,(const char *)p+off,4);return x;}
static int integer(const void *p,unsigned off){int x;memcpy(&x,(const char *)p+off,4);return x;}
#include "profiles.h"
#include "catalog.h"
static void clear_state(void){als_reset(&state);InterlockedExchangePointer(&pop_identity,NULL);InterlockedExchangePointer(&effect_car,NULL);InterlockedExchangePointer(&audio_snapshot,NULL);InterlockedExchange64(&burst_until,0);last_physics=0;last_counter.QuadPart=0;}
static void guard_log(unsigned flag,const char *message){if(!(guard_mask&flag)){guard_mask|=flag;log_line(message);}}
static int burst_active(void){return als_burst_alive((ULONGLONG)InterlockedCompareExchange64(&burst_until,0,0),GetTickCount64(),InterlockedCompareExchange(&enabled,0,0));}
static void snapshot(void *car,void *frame){
    snapshot_next(car,frame);
    if(burst_active() && car==InterlockedCompareExchangePointer(&effect_car,NULL,NULL) && readable(frame,0x16a)){
        // The verified native snapshot assigns this exact byte from car+4e2c.
        // Reassert on the consumer's copy, so the simulation reset cannot lose it.
        *((unsigned char *)frame+0x169)=1;
        InterlockedExchangePointer(&audio_snapshot,frame);
        if(InterlockedIncrement(&snapshot_count)<=12)log_line("ANTI-LAG AUDIO SNAPSHOT: backfire flag delivered to native frame.\r\n");
    }
}
static void visual(void *self,void *transforms){
    if(burst_active() && InterlockedIncrement(&visual_count)<=12)
        log_line("ANTI-LAG VISUAL CONSUMER: native car_backfire routine reached during burst (not proof of visible particles).\r\n");
    visual_next(self,transforms);
}
static void audio(void *self,void *frame,void *a,void *b,void *c,void *d,void *e){
    int observed=burst_active() && frame==InterlockedCompareExchangePointer(&audio_snapshot,NULL,NULL) && readable(frame,0x16a) && *((unsigned char *)frame+0x169);
    audio_next(self,frame,a,b,c,d,e);
    if(observed && InterlockedIncrement(&audio_count)<=12)
        log_line("ANTI-LAG AUDIO CONSUMER: native engine-group update received the backfire flag (not proof of audible playback).\r\n");
}
static void burble(void *self,float dt,void *physics_ref){
    // Clean our preceding pulse BEFORE native processing, even when disabled,
    // no longer holding, or input has become inactive. Level-zero native paths
    // can return without resetting this event field themselves.
    int layout=readable(self,16) && *(void **)self==image+0x2e6b080;
    if(layout){
        int before=integer(self,12);
        int after=als_pulse_clear(&native_pulse,(uintptr_t)self,before);
        if(after!=before)memcpy((char *)self+12,&after,4);
    }
    burble_next(self,dt,physics_ref);
    // Native routine has already updated/released its borrowed physics reference.
    // Only compare the identity; never retain or dereference a borrowed car.
    if(InterlockedCompareExchange(&enabled,0,0) &&
       burst_active() && InterlockedCompareExchangePointer(&pop_identity,NULL,NULL)==self &&
       layout){
        // 2 is a native backfire event code, emitted by the original routine.
        // Preserve a native event already scheduled this frame.
        int before=integer(self,12);
        int event=als_pulse_emit(&native_pulse,(uintptr_t)self,before);
        if(event!=before){memcpy((char *)self+12,&event,4);
            if(++pops<=8)log_line("ANTI-LAG: one-update native backfire pulse scheduled.\r\n");}
    }
}
void *entry_hook_install(unsigned char *,size_t,uint32_t,const unsigned char *,unsigned,void *,void **);
#include "turbo_ui.h"
__declspec(dllexport) BOOL WINAPI FH2PluginInitialize(const FH2PluginContext *ctx){
    if(!ctx || ctx->size<sizeof(*ctx) || ctx->api_version!=1 || ctx->adapter!=FH2_PLUGIN_AZIMUTH ||
       ctx->game_image_size!=0x3a18000 || !ctx->game_image || !ctx->extra_options_version || strcmp(ctx->extra_options_version,"0.4.4"))return FALSE;
    context=ctx;image=ctx->game_image;
    for(unsigned i=0;i<sizeof signatures/sizeof *signatures;i++)if(memcmp(image+signatures[i].rva,signatures[i].bytes,16)){
        log_line("ANTI-LAG: native code changed or owned by another mod; initialization refused.\r\n");return FALSE;
    }
    wchar_t ini[32768],log[32768];
    if(!ctx->plugins_directory || wcslen(ctx->plugins_directory)>32000)return FALSE;
    wcscpy(ini,ctx->plugins_directory);wcscat(ini,L"\\EO_AntiLag.ini");
    wcscpy(log,ctx->plugins_directory);wcscat(log,L"\\EO_AntiLag.log");
    logfile=CreateFileW(log,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    int ms=(int)GetPrivateProfileIntW(L"AntiLag",L"PressureDecayMs",6000,ini);
    int percent=(int)GetPrivateProfileIntW(L"AntiLag",L"RetentionPercent",85,ini);
    int rpm=(int)GetPrivateProfileIntW(L"AntiLag",L"CutoffRPM",1800,ini);
    int popms=(int)GetPrivateProfileIntW(L"AntiLag",L"PopIntervalMs",140,ini);
    if(ms<1000 || ms>8000 || percent<25 || percent>95 || rpm<1000 || rpm>6000 || popms<100 || popms>500){log_line("ANTI-LAG: invalid configuration; initialization refused.\r\n");return FALSE;}
    duration=ms/1000.0f;retain=percent/100.0f;min_rpm=(float)rpm;interval=popms/1000.0f;
    profiles_load(ctx->plugins_directory,(ALSSettings){ms,percent,rpm,popms});
    if(!QueryPerformanceFrequency(&counter_frequency) || counter_frequency.QuadPart<=0){log_line("ANTI-LAG: performance clock unavailable; refusing activation.\r\n");return FALSE;}
    const unsigned hook_rvas[]={0xf7cf90,0x13b0d70,0x1026b80,0xf7d8a0};
    const unsigned lengths[]={8,8,10,8};
    void *handlers[]={(void *)snapshot,(void *)visual,(void *)audio,(void *)burble};
    void **originals[]={(void **)&snapshot_next,(void **)&visual_next,(void **)&audio_next,(void **)&burble_next};
    for(unsigned h=0;h<4;h++){
        const unsigned char *expected=NULL;
        for(unsigned i=0;i<sizeof signatures/sizeof *signatures;i++)if(signatures[i].rva==hook_rvas[h])expected=signatures[i].bytes;
        if(!expected || !entry_hook_install(image,ctx->game_image_size,hook_rvas[h],expected,lengths[h],handlers[h],originals[h])){
        char message[140];snprintf(message,sizeof message,"ANTI-LAG: hook at game+0x%x refused; plugin disabled.\r\n",hook_rvas[h]);log_line(message);return FALSE;
        }
    }
    InterlockedExchange(&enabled,1);ready=1;
    turbo_install(ctx->game_image_size);
    log_line("ANTI-LAG 0.6.8 experimental initialized; per-car settings and native defaults reset; TURBO menu; special turbo required; always active when equipped; no toggle key.\r\n");return TRUE;
}
__declspec(dllexport) void FH2PluginVehicleUpdate(const FH2PluginVehicleFrame *frame){
    if(!ready || !frame || frame->size<sizeof(*frame))return;
    // Consumer window survives several simulation updates, but never pedal return.
    void *car=frame->car;
    if(!frame->input_enabled){guard_log(1,"ANTI-LAG GUARD: gameplay input inactive; resetting.\r\n");clear_state();return;}
    if(!InterlockedCompareExchange(&enabled,0,0)){clear_state();return;}
    if(!readable(car,0x4c10) || *(void **)car!=image+0x2c5cb80){guard_log(2,"ANTI-LAG GUARD: local car layout mismatch.\r\n");clear_state();return;}
    if(!catalog_equipped(car)){guard_log(16,"ANTI-LAG SHOP: special turbo not equipped or catalog unavailable; anti-lag inactive.\r\n");clear_state();return;}
    guard_log(32,"ANTI-LAG SHOP: special workshop turbo equipped; anti-lag available.\r\n");
    void *physics=*(void **)((char *)car+0x4c08);
    if(!readable(physics,0x11b0) || *(void **)physics!=image+0x2eb5e18){guard_log(4,"ANTI-LAG GUARD: dynamics layout mismatch.\r\n");clear_state();return;}
    if(last_physics!=(uintptr_t)physics){clear_state();last_physics=(uintptr_t)physics;}
    int model=profile_model(integer(car,0x128));
    static LONG selected_revision=-1;LONG revision=InterlockedCompareExchange(&profile_revision,0,0);
    if(model!=selected_model||revision!=selected_revision){
        selected_revision=revision;
        clear_state();last_physics=(uintptr_t)physics;selected_model=model;
        int custom=0;ALSSettings settings=profiles_choose(model,&custom);
        duration=settings.decay/1000.0f;retain=settings.retention/100.0f;min_rpm=(float)settings.cutoff;interval=settings.pop/1000.0f;
        diagnostic_count=0;next_diagnostic=0;
        char line[256];snprintf(line,sizeof line,"ANTI-LAG PROFILE: model=%d source=%s PressureDecayMs=%d RetentionPercent=%d CutoffRPM=%d PopIntervalMs=%d\r\n",model,custom?"car":"global",settings.decay,settings.retention,settings.cutoff,settings.pop);log_line(line);
    }
    LARGE_INTEGER now;if(!QueryPerformanceCounter(&now)){clear_state();return;}
    float elapsed=last_counter.QuadPart?(float)((double)(now.QuadPart-last_counter.QuadPart)/(double)counter_frequency.QuadPart):0;
    last_counter=now;
    ALSInput in={elapsed,number(physics,0x1190),number(physics,0x148)*9.5492966f,
        number(physics,0x168),number(physics,0x170),number(physics,0xa38),number(physics,0xa40),1,1,
        integer(physics,0xa24)>0 && integer(physics,0xa24)<=4};
    ALSOutput out=als_tick(&state,in,duration,retain,min_rpm,interval);
    if(!out.hold){InterlockedExchange64(&burst_until,0);InterlockedExchangePointer(&effect_car,NULL);InterlockedExchangePointer(&pop_identity,NULL);InterlockedExchangePointer(&audio_snapshot,NULL);}
    ULONGLONG clock_now=GetTickCount64();
    if(clock_now>=next_diagnostic && diagnostic_count<60 && (in.throttle>0.2f || state.charged || diagnostic_count<6)){
        next_diagnostic=clock_now+500;diagnostic_count++;
        char line[400];snprintf(line,sizeof line,"ANTI-LAG SAMPLE: raw_dt=%.6g measured_s=%.6g throttle=%.3f rpm=%.0f turbo=%d spool=%.3f/%.3f pressure=%.3f/%.3f charged=%d remaining=%.3f hold=%d pop=%d\r\n",
            (double)frame->delta_seconds,(double)elapsed,(double)in.throttle,(double)in.rpm,in.turbo,
            (double)in.spool,(double)in.max_spool,(double)in.pressure,(double)in.max_pressure,state.charged,(double)state.remaining,out.hold,out.pop);
        log_line(line);
    }
    if(out.hold){
        // Restore only a floor below the previously attained native boost.
        // Never write throttle, RPM, engine torque, tune settings or save data.
        memcpy((char *)physics+0x168,&out.spool,4);memcpy((char *)physics+0x170,&out.pressure,4);
        if(++holds<=8)log_line("ANTI-LAG: retaining native turbo state on lift.\r\n");
        if(out.pop && readable((char *)car+0x4e20,16) && *(void **)((char *)car+0x4e20)==image+0x2e6b080){
            InterlockedExchangePointer(&effect_car,car);
            InterlockedExchangePointer(&pop_identity,(char *)car+0x4e20);
            InterlockedExchange64(&burst_until,(LONG64)als_burst_deadline(GetTickCount64()));
        }
    }
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved){(void)instance;(void)reason;(void)reserved;return TRUE;}
