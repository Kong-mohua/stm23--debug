#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "settings.h"
#include "race.h"
static uint8_t pages[2][1024];
static uint8_t fail_write;
void Store_Read(uint8_t slot, void *out, uint16_t n) { memcpy(out, pages[slot], n); }
uint8_t Store_Write(uint8_t slot, const void *data, uint16_t n)
{
    memset(pages[slot], 0xFF, 1024);
    memcpy(pages[slot], data, fail_write ? n / 2 : n);
    return !fail_write;
}
int main(void)
{
    memset(pages, 0xFF, sizeof(pages)); Settings_Load();
    assert(Settings_Valid(&settings) && settings.speed == 80 && !settings.marker_enabled);
    assert(Settings_Save());
    settings.speed = 65; assert(Settings_Save());
    settings.speed = 40; fail_write = 1; assert(!Settings_Save());
    Settings_Load(); assert(settings.speed == 65); /* incomplete new page ignored */
    fail_write = 0; settings.speed = 70; assert(Settings_Save());
    pages[0][16] ^= 1; Settings_Load(); assert(settings.speed == 65); /* CRC fallback */
    settings.laps = 3; assert(!Settings_Save()); Settings_Defaults();
    assert(!Race_Start(0, 1, 0x18)); /* calibration required */
    settings.calibrated = 1;
    assert(!Race_Start(0, 0, 0x18));
    assert(!Race_Start(0, 1, 0));
    assert(Race_Start(0, 1, 0x18)); Race_Update(5, 1, 0x18);
    assert(race.left == 80 && race.right == 80);
    Race_Update(10, 1, 0x80); assert(race.state == RACE_CORNER && race.left == 80 && race.right == 0);
    Race_Update(15, 1, 0x81); assert(race.state == RACE_BRANCH && race.error == 100);
    Race_Update(20, 1, 0xFF); assert(race.state == RACE_WIDE && race.error == 100 && !race.laps);
    Race_Update(30, 1, 0); assert(race.state == RACE_GAP && race.running);
    Race_Update(371, 1, 0); assert(race.state == RACE_FAULT && !race.running && !race.left);
    assert(Race_Start(400, 1, 0x18)); Race_Update(405, 0, 0x18);
    assert(!race.running && race.state == RACE_FAULT);
    settings.reverse = 1; assert(Race_Start(0, 1, 0x18)); Race_Update(5, 1, 0x80);
    assert(race.error == -100 && race.left == 0 && race.right == 80);
    Settings_Defaults(); settings.calibrated = 1; settings.marker_enabled = 1; settings.laps = 2;
    assert(Race_Start(0, 1, 0xFF)); Race_Update(6000, 1, 0xFF);
    assert(!race.laps); /* waiting on starting marker is never a lap */
    Race_Update(6010, 1, 0x18); Race_Update(6020, 1, 0xFF);
    Race_Update(6060, 1, 0xFF); assert(!race.laps); /* brief release does not arm */
    Race_Update(6070, 1, 0x18); Race_Update(6110, 1, 0x18);
    Race_Update(6120, 1, 0xFF); Race_Update(6140, 1, 0xFF); assert(!race.laps);
    Race_Update(6150, 1, 0xFF); assert(race.laps == 1 && race.running && race.beep);
    race.beep = 0; Race_Update(12000, 1, 0xFF); assert(race.laps == 1);
    Race_Update(12010, 1, 0x18); Race_Update(12050, 1, 0x18);
    Race_Update(12060, 1, 0xFF); Race_Update(12090, 1, 0xFF);
    assert(race.laps == 2 && !race.running && race.state == RACE_DONE && race.elapsed == 12090);
    Race_Update(13000, 1, 0x18); assert(race.elapsed == 12090); /* timer frozen */
    settings.laps = 1;
    uint32_t start = UINT32_MAX - 100;
    assert(Race_Start(start, 1, 0x18)); Race_Update(6000, 1, 0xFF); Race_Update(6030, 1, 0xFF);
    assert(!race.running && race.elapsed == (uint32_t)(6030u - start));
    puts("PASS: settings CRC/power-loss recovery/ranges, startup refusal, corners/branches/gaps, ADC fault, polarity direction, lap debounce/count/timer/wrap");
    return 0;
}
