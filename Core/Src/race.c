#include "race.h"
#include <string.h>
Race race;
static const int16_t weights[8] = {-100, -71, -43, -14, 14, 43, 71, 100};
static int16_t clamp(int16_t x) { return x < 0 ? 0 : (x > 100 ? 100 : x); }
void Race_Stop(uint32_t now)
{
    if (race.running) race.elapsed = now - race.start_tick;
    race.running = 0; race.left = race.right = 0;
    if (race.state != RACE_DONE && race.state != RACE_FAULT) race.state = RACE_IDLE;
}
uint8_t Race_Start(uint32_t now, uint8_t valid, uint8_t mask)
{
    memset(&race, 0, sizeof(race));
    if (!valid || !mask || !settings.calibrated || !Settings_Valid(&settings)) {
        race.state = RACE_FAULT; return 0;
    }
    race.running = 1; race.beep = 1; race.state = RACE_LINE;
    race.start_tick = race.last_line = race.last_lap = now;
    /* The starting bar must be left before a return can count as a lap. */
    race.marker_active = (mask == 0xFFu);
    race.marker_tick = now;
    race.marker_counted = race.marker_active;
    race.marker_armed = !race.marker_active;
    return 1;
}
static uint8_t reverse_bits(uint8_t x)
{
    uint8_t y = 0;
    for (uint8_t i = 0; i < 8; ++i) { y = (uint8_t)((y << 1) | (x & 1u)); x >>= 1; }
    return y;
}
void Race_Update(uint32_t now, uint8_t valid, uint8_t mask)
{
    int16_t corr, error = 0;
    uint8_t count = 0, groups = 0;
    if (!race.running) return;
    race.elapsed = now - race.start_tick;
    if (!valid) { race.state = RACE_FAULT; Race_Stop(now); return; }
    if (settings.reverse) mask = reverse_bits(mask);
    if (mask) race.last_line = now;
    if (mask == 0xFFu) {
        race.release_pending = 0;
        if (!race.marker_active) {
            race.marker_active = 1; race.marker_counted = 0; race.marker_tick = now;
        }
        if (settings.marker_enabled && race.marker_armed && !race.marker_counted &&
            now - race.marker_tick >= settings.marker_ms &&
            now - race.last_lap >= settings.min_lap_ms) {
            race.marker_counted = 1; race.marker_armed = 0;
            race.last_lap = now; ++race.laps; race.beep = 1;
            if (race.laps >= settings.laps) {
                race.state = RACE_DONE; Race_Stop(now); return;
            }
        }
    } else {
        if (!race.release_pending) { race.release_pending = 1; race.release_tick = now; }
        if (!race.marker_active || now - race.release_tick >= settings.marker_ms) {
            race.marker_active = race.marker_counted = 0;
            race.marker_armed = 1;
        }
    }
    if (!mask) {
        race.state = RACE_GAP;
        if (now - race.last_line > settings.gap_ms) {
            race.state = RACE_FAULT; Race_Stop(now); return;
        }
        /* A bounded, slow crossing of a missing section, rather than
           blindly continuing at 80% for 1.5 seconds. */
        corr = (int16_t)(race.error * (int16_t)settings.gain / 400);
        race.left = clamp((int16_t)(settings.speed * 3 / 4) + corr);
        race.right = clamp((int16_t)(settings.speed * 3 / 4) - corr);
        return;
    }
    for (uint8_t i = 0; i < 8; ++i) {
        if (mask & (1u << i)) {
            ++count;
            if (!i || !(mask & (1u << (i - 1u)))) ++groups;
        }
    }
    if (count >= 5u) {
        /* Broad junction: carry the previous course across it. */
        race.state = RACE_WIDE;
        corr = (int16_t)(race.error * (int16_t)settings.gain / 400);
        race.left = clamp((int16_t)(settings.speed * 3 / 4) + corr);
        race.right = clamp((int16_t)(settings.speed * 3 / 4) - corr);
        return;
    }
    /* Keep separate clusters separate: averaging two branches creates a
       fictitious center line. Choose the cluster closest to the previous
       error. Ties prefer the center, then the left branch. */
    {
        int16_t best_distance = 32767;
        for (uint8_t i = 0; i < 8;) {
            int16_t sum = 0, n = 0, candidate, distance;
            if (!(mask & (1u << i))) { ++i; continue; }
            do { sum += weights[i]; ++n; ++i; }
            while (i < 8u && (mask & (1u << i)));
            candidate = (int16_t)(sum / n);
            distance = candidate - race.error;
            if (distance < 0) distance = (int16_t)-distance;
            if (distance < best_distance) { best_distance = distance; error = candidate; }
        }
    }
    race.error = error;
    race.state = groups > 1u ? RACE_BRANCH : RACE_LINE;
    if (groups == 1u && !(mask & 0x3Cu) && (error <= -71 || error >= 71)) {
        /* Tight corner: pause the inner wheel; do not reverse/spin. */
        race.state = RACE_CORNER;
        race.left = error < 0 ? 0 : settings.speed;
        race.right = error < 0 ? settings.speed : 0;
    } else {
        corr = (int16_t)(error * (int16_t)settings.gain / 100);
        race.left = clamp((int16_t)settings.speed + corr);
        race.right = clamp((int16_t)settings.speed - corr);
    }
}
const char *Race_StateName(void)
{
    static const char *const names[] = {"IDLE", "LINE", "WIDE", "BRANCH",
                                        "CORNER", "GAP", "DONE", "FAULT"};
    return names[race.state];
}
