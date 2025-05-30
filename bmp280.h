#ifndef __BMP280_H__
#define __BMP280_H__

#include "stm32f1xx_hal.h"

// Dirección de registros BMP280
#define BMP280_REG_ID           0xD0
#define BMP280_REG_RESET        0xE0
#define BMP280_REG_STATUS       0xF3
#define BMP280_REG_CTRL_MEAS    0xF4
#define BMP280_REG_CONFIG       0xF5
#define BMP280_REG_PRESS_MSB    0xF7
#define BMP280_REG_CALIB00      0x88

// Nueva definición faltante
#define BMP280_REG_CONTROL      BMP280_REG_CTRL_MEAS
#define BMP280_REG_CALIB        BMP280_REG_CALIB00

// Estructura de coeficientes de calibración
typedef struct {
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
} BMP280_CalibData_t;

// Estructura de dispositivo BMP280
typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    BMP280_CalibData_t calib;
    int32_t t_fine;
} BMP280_t;

// Estructura de datos de medición
typedef struct {
    int32_t temperature; // en centésimas de °C
    uint32_t pressure;   // en Pa
} BMP280_Data_t;

// Declaraciones de funciones
uint8_t BMP280_Init(BMP280_t *dev);
void BMP280_Read(BMP280_t *dev, BMP280_Data_t *data);
uint8_t BMP280_ReadID(BMP280_t *dev);
void BMP280_ReadRegs(BMP280_t *dev, uint8_t reg, uint8_t *buf, uint16_t len);
void BMP280_WriteReg(BMP280_t *dev, uint8_t reg, uint8_t value);

#endif // __BMP280_H__
