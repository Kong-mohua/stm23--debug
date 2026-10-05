#ifndef SETTINGS_H
#define SETTINGS_H
#include <stdint.h>
typedef struct {
    uint16_t threshold[8];
    uint16_t speed, gain, gap_ms, min_lap_ms, marker_ms;
    uint8_t laps, polarity, reverse, marker_enabled, calibrated;
} Settings;
extern Settings settings;
void Settings_Defaults(void);
uint8_t Settings_Valid(const Settings *s);
void Settings_Load(void);
uint8_t Settings_Save(void); /* caller must stop motors first */
/* Two reserved 1 KiB pages. Platform backend or host fake. */
void Store_Read(uint8_t slot, void *out, uint16_t length);
uint8_t Store_Write(uint8_t slot, const void *data, uint16_t length);
#endif
