#include "settings.h"
#include <string.h>
#define MAGIC 0x53544D32u
#define VERSION 1u
typedef struct {
    uint32_t magic, version, sequence;
    Settings data;
    uint32_t crc;
} Record;
Settings settings;
static uint32_t sequence;
static uint8_t current_slot, have_record;
static uint32_t checksum(const Record *r)
{
    const uint8_t *p = (const uint8_t *)r;
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < (uint32_t)((const uint8_t *)&r->crc - p); ++i) {
        crc ^= p[i];
        for (uint8_t b = 0; b < 8; ++b)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return ~crc;
}
uint8_t Settings_Valid(const Settings *s)
{
    if (s->speed < 30u || s->speed > 100u || s->gain > 100u ||
        s->gap_ms < 50u || s->gap_ms > 1000u ||
        s->min_lap_ms < 2000u || s->min_lap_ms > 60000u ||
        s->marker_ms < 10u || s->marker_ms > 500u ||
        s->laps < 1u || s->laps > 2u || s->polarity > 1u ||
        s->reverse > 1u || s->marker_enabled > 1u || s->calibrated > 1u) return 0;
    for (uint8_t i = 0; i < 8; ++i)
        if (s->threshold[i] < 1u || s->threshold[i] > 4094u) return 0;
    return 1;
}
void Settings_Defaults(void)
{
    memset(&settings, 0, sizeof(settings));
    for (uint8_t i = 0; i < 8; ++i) settings.threshold[i] = 2048;
    settings.speed = 80; settings.gain = 35; settings.gap_ms = 350;
    settings.min_lap_ms = 5000; settings.marker_ms = 30;
    settings.laps = 1; settings.polarity = 1;
    /* Wide black bars also occur at intersections: enable only after the
       actual start/finish marker has been confirmed on the physical track. */
    settings.marker_enabled = 0;
}
static uint8_t record_valid(const Record *r)
{
    return r->magic == MAGIC && r->version == VERSION &&
           r->crc == checksum(r) && Settings_Valid(&r->data);
}
void Settings_Load(void)
{
    Record a, b;
    uint8_t va, vb;
    Settings_Defaults();
    Store_Read(0, &a, sizeof(a)); Store_Read(1, &b, sizeof(b));
    va = record_valid(&a); vb = record_valid(&b);
    have_record = va || vb; sequence = 0; current_slot = 0;
    if (vb && (!va || (int32_t)(b.sequence - a.sequence) > 0)) {
        settings = b.data; sequence = b.sequence; current_slot = 1;
    } else if (va) { settings = a.data; sequence = a.sequence; }
}
uint8_t Settings_Save(void)
{
    Record r, check;
    uint8_t slot = have_record ? (uint8_t)(current_slot ^ 1u) : 0u;
    if (!Settings_Valid(&settings)) return 0;
    memset(&r, 0, sizeof(r));
    r.magic = MAGIC; r.version = VERSION; r.sequence = sequence + 1u;
    r.data = settings; r.crc = checksum(&r);
    if (!Store_Write(slot, &r, sizeof(r))) return 0;
    Store_Read(slot, &check, sizeof(check));
    if (!record_valid(&check) || memcmp(&r, &check, sizeof(r))) return 0;
    current_slot = slot; sequence = r.sequence; have_record = 1;
    return 1;
}
