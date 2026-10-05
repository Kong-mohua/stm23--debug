#include "board.h"
#include "settings.h"
#include "main.h"
#include <string.h>
#define STORE_BASE 0x0800F800u
#define STORE_PAGE 1024u
static uint8_t beeping;
static uint32_t beep_tick;
static void buzzer(uint8_t on)
{
#if BUZZER_ENABLED
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9,
        (on == BUZZER_ACTIVE_HIGH) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
    (void)on;
#endif
}
void Board_Init(void)
{
#if BUZZER_ENABLED
    GPIO_InitTypeDef gi = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    buzzer(0);
    gi.Pin = GPIO_PIN_9; gi.Mode = GPIO_MODE_OUTPUT_PP;
    gi.Pull = GPIO_NOPULL; gi.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gi);
#endif
    beeping = 0;
}
void Board_Beep(uint32_t now) { beep_tick = now; beeping = 1; buzzer(1); }
void Board_Tick(uint32_t now)
{
    if (beeping && now - beep_tick >= 100u) { buzzer(0); beeping = 0; }
}
void Store_Read(uint8_t slot, void *out, uint16_t length)
{
    if (slot > 1u || length > STORE_PAGE) { memset(out, 0xFF, length); return; }
    memcpy(out, (const void *)(uintptr_t)(STORE_BASE + slot * STORE_PAGE), length);
}
uint8_t Store_Write(uint8_t slot, const void *data, uint16_t length)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t error, address;
    const uint8_t *p = data;
    uint8_t ok = 1;
    if (slot > 1u || length < 4u || length > STORE_PAGE || (length & 1u)) return 0;
    address = STORE_BASE + slot * STORE_PAGE;
    if (HAL_FLASH_Unlock() != HAL_OK) return 0;
    erase.TypeErase = FLASH_TYPEERASE_PAGES; erase.PageAddress = address; erase.NbPages = 1;
    if (HAL_FLASHEx_Erase(&erase, &error) != HAL_OK) ok = 0;
    /* Write the magic last: interrupted writes cannot look committed.
       Only the inactive page is erased; the last saved page remains valid. */
    for (uint16_t i = 4; ok && i < length; i += 2) {
        uint16_t half = (uint16_t)(p[i] | ((uint16_t)p[i + 1u] << 8));
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address + i, half) != HAL_OK) ok = 0;
    }
    for (uint16_t i = 0; ok && i < 4u; i += 2) {
        uint16_t half = (uint16_t)(p[i] | ((uint16_t)p[i + 1u] << 8));
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address + i, half) != HAL_OK) ok = 0;
    }
    if (HAL_FLASH_Lock() != HAL_OK) ok = 0;
    return ok;
}
