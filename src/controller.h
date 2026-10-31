/* GPL-3.0. Standalone anti-lag policy; no game memory access here. */
#ifndef ANTILAG_CONTROLLER_H
#define ANTILAG_CONTROLLER_H
typedef struct {float spool,pressure,remaining,pop_clock,initial_spool,initial_boost,age,charge_age;int charged,lifted;} ALSState;
typedef struct {float dt,throttle,rpm,spool,pressure,max_spool,max_pressure;int enabled,can_drive,turbo;} ALSInput;
typedef struct {float spool,pressure;int hold,pop;float intensity;} ALSOutput;
static void als_reset(ALSState *s){*s=(ALSState){0};}
static int als_finite(float x){return x==x && x>-1e20f && x<1e20f;}
static ALSOutput als_tick(ALSState *s,ALSInput in,float duration,float retain,float min_rpm,float interval){
    ALSOutput out={in.spool,in.pressure,0,0,0};
    if(!in.enabled || !in.can_drive || !in.turbo || !(in.dt>0 && in.dt<=0.1f) ||
       !als_finite(in.throttle) || in.throttle<0 || in.throttle>1 ||
       !als_finite(in.rpm) || in.rpm<min_rpm || in.rpm>20000 ||
       !als_finite(in.spool) || !als_finite(in.pressure) || !als_finite(in.max_spool) || !als_finite(in.max_pressure) ||
       in.max_spool<=0 || in.max_spool>1e7f || in.max_pressure<=14.7f || in.max_pressure>500 ||
       in.spool<0 || in.spool>in.max_spool*1.02f || in.pressure<0 || in.pressure>in.max_pressure*1.02f){als_reset(s);return out;}
    // Any meaningful return to the pedal cancels the preceding lift window.
    if(in.throttle>0.10f){
        if(s->lifted)als_reset(s);
        // Smooth pedal release crosses this band before reaching zero. Keep
        // the most recent valid charge for at most 350ms, without any effect
        // while the pedal is still above the lift threshold.
        if(in.throttle<0.30f || in.rpm<min_rpm+700.0f || in.spool<in.max_spool*0.25f || in.pressure<15.7f){
            s->charge_age+=in.dt;if(s->charge_age>0.35f)als_reset(s);return out;
        }
        als_reset(s);
        s->spool=in.spool<in.max_spool?in.spool:in.max_spool;
        s->pressure=in.pressure<in.max_pressure?in.pressure:in.max_pressure;
        s->charged=1;
        s->initial_spool=s->spool;s->initial_boost=s->pressure-14.7f;
        s->remaining=duration;s->lifted=0;s->pop_clock=0;return out;
    }
    if(!s->charged)return out;
    if(!(duration>=0.1f && duration<=8.0f && retain>0 && retain<=1 && interval>=0.1f && interval<=0.5f)){als_reset(s);return out;}
    s->age+=in.dt;
    s->spool-=s->initial_spool*in.dt/duration;
    s->pressure-=s->initial_boost*in.dt/duration;
    s->remaining=duration-s->age; // diagnostic estimate, not the activation gate
    if(s->age>=10.0f || s->pressure<=15.7f || s->spool<=in.max_spool*0.05f){als_reset(s);return out;}
    if(!s->lifted){s->lifted=1;s->pop_clock=interval;}
    float floor_spool=s->spool*retain;
    float floor_pressure=14.7f+(s->pressure-14.7f)*retain;
    // Native limits always win, including an engine/car configuration change.
    if(floor_spool>in.max_spool)floor_spool=in.max_spool;
    if(floor_pressure>in.max_pressure)floor_pressure=in.max_pressure;
    if(out.spool<floor_spool)out.spool=floor_spool;
    if(out.pressure<floor_pressure)out.pressure=floor_pressure;
    out.hold=1;
    float rpm_factor=(in.rpm-min_rpm)/2000.0f;if(rpm_factor>1)rpm_factor=1;
    out.intensity=(s->pressure-14.7f)/(in.max_pressure-14.7f)*rpm_factor;
    if(out.intensity>1)out.intensity=1;
    float pulse_interval=interval*(1.0f+2.0f*(1.0f-out.intensity));
    s->pop_clock+=in.dt;
    if(s->pop_clock>=pulse_interval){s->pop_clock=0;out.pop=1;}
    return out;
}
#endif
