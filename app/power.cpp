#include "power.hpp"

#include "audio.hpp"
#include "backlight.hpp"
#include "peripherals.hpp"

namespace power {

void init() { HAL_PWREx_DisablePullUpPullDownConfig(); }

void shut_down(led::Matrix& matrix, lcd::Display& screen) {
    // Roadmap order: speaker, LED board, display, backlight, then the MCU.
    audio::end();  // DAC to mid-scale, then AMP_EN low (no-op if it isn't on)
    matrix.shutdown();
    screen.sleep();
    backlight::set_percent(0);
    HAL_GPIO_WritePin(LOW_BAT_LED_GPIO_Port, LOW_BAT_LED_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AMP_EN_GPIO_Port, AMP_EN_Pin, GPIO_PIN_RESET);

    // In Shutdown mode every pin floats unless it has an external resistor or the PWR block
    // holds it. From the schematic, these already have external parts:
    //   PA1 LOW_BAT_LED (270R + LED to GND), PA2 AMP_EN (R19 100k down), PA8 backlight gate
    //   (R24 100k down), PB7 DISPLAY_nRST (R11 10k up), I2C (R4/R5 4.7k up), IS31 SDB (R9 20k up),
    //   encoder A/B/switch (R2/R6/R1 10k up).
    // The display's other lines have none, so hold them here:
    //   PB8 DISPLAY_CS: high, so it stays deselected and asleep.
    //   PB9 A0, PB10 SCK, PB11 MOSI: low, so the display's CMOS inputs don't float.
    HAL_PWREx_EnableGPIOPullUp(PWR_GPIO_B, PWR_GPIO_BIT_8);
    HAL_PWREx_EnableGPIOPullDown(PWR_GPIO_B, PWR_GPIO_BIT_9 | PWR_GPIO_BIT_10 | PWR_GPIO_BIT_11);
    HAL_PWREx_EnablePullUpPullDownConfig();

    // No wakeup pins are enabled (PA5, the encoder switch, isn't one), so clear any stale flags
    // and go. Nothing after this runs.
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WUF);
    HAL_SuspendTick();
    HAL_PWREx_EnterSHUTDOWNMode();
    for (;;) {
    }
}

}  // namespace power
