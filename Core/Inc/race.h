#ifndef RACE_H
#define RACE_H
#include "settings.h"
typedef enum { RACE_IDLE, RACE_LINE, RACE_WIDE, RACE_BRANCH,
               RACE_CORNER, RACE_GAP, RACE_DONE, RACE_FAULT } RaceState;
typedef struct {
    RaceState state;
    uint8_t running, laps, beep, marker_armed, marker_active, marker_counted, release_pending;
    uint32_t start_tick, last_line, last_lap, marker_tick, release_tick, elapsed;
    int16_t left, right, error;
} Race;
extern Race race;
uint8_t Race_Start(uint32_t now, uint8_t valid, uint8_t mask);
void Race_Stop(uint32_t now);
void Race_Update(uint32_t now, uint8_t valid, uint8_t mask);
const char *Race_StateName(void);
#endif
