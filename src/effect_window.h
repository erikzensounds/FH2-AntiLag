/* GPL-3.0. Bounded consumer window, using monotonic milliseconds. */
#ifndef ALS_EFFECT_WINDOW_H
#define ALS_EFFECT_WINDOW_H
#include <stdint.h>
static inline uint64_t als_burst_deadline(uint64_t now){return now+80;}
static inline int als_burst_alive(uint64_t until,uint64_t now,int enabled){return enabled && now<until;}
#endif
