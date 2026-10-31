/* GPL-3.0. Ownership of a one-update native backfire pulse.
   Identity is compared only; no stored game pointer is dereferenced here. */
#ifndef ALS_NATIVE_PULSE_H
#define ALS_NATIVE_PULSE_H
#include <stdint.h>
typedef struct {uintptr_t identity;int value;} ALSNativePulse;
static int als_pulse_clear(ALSNativePulse *pulse,uintptr_t identity,int current){
    if(pulse->identity!=identity)return current;
    int result=current==pulse->value?0:current;
    *pulse=(ALSNativePulse){0};return result;
}
static int als_pulse_emit(ALSNativePulse *pulse,uintptr_t identity,int current){
    if(current || !identity)return current;
    pulse->identity=identity;pulse->value=2;return 2;
}
#endif
