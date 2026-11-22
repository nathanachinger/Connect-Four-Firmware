#pragma once

// CubeMX peripheral handles defined in Core/Src/main.c.

#include "main.h"

extern "C" {
extern ADC_HandleTypeDef hadc1;
extern DAC_HandleTypeDef hdac1;
extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi2;
extern TIM_HandleTypeDef htim3;
}
