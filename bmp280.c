#include "bmp280.h"

uint8_t BMP280_Init(BMP280_t *dev)
{
    uint8_t calib[24];
    BMP280_ReadRegs(dev, BMP280_REG_CALIB, calib, 24);

    dev->calib.dig_T1 = (uint16_t)(calib[1] << 8 | calib[0]);
    dev->calib.dig_T2 = (int16_t)(calib[3] << 8 | calib[2]);
    dev->calib.dig_T3 = (int16_t)(calib[5] << 8 | calib[4]);
    dev->calib.dig_P1 = (uint16_t)(calib[7] << 8 | calib[6]);
    dev->calib.dig_P2 = (int16_t)(calib[9] << 8 | calib[8]);
    dev->calib.dig_P3 = (int16_t)(calib[11] << 8 | calib[10]);
    dev->calib.dig_P4 = (int16_t)(calib[13] << 8 | calib[12]);
    dev->calib.dig_P5 = (int16_t)(calib[15] << 8 | calib[14]);
    dev->calib.dig_P6 = (int16_t)(calib[17] << 8 | calib[16]);
    dev->calib.dig_P7 = (int16_t)(calib[19] << 8 | calib[18]);
    dev->calib.dig_P8 = (int16_t)(calib[21] << 8 | calib[20]);
    dev->calib.dig_P9 = (int16_t)(calib[23] << 8 | calib[22]);

    BMP280_WriteReg(dev, BMP280_REG_CONTROL, 0x3F); // Oversampling presión x1, temperatura x1, modo normal

    return 0;
}

void BMP280_Read(BMP280_t *dev, BMP280_Data_t *data)
{
    uint8_t buf[6];
    BMP280_ReadRegs(dev, BMP280_REG_PRESS_MSB, buf, 6);

    int32_t adc_P = (int32_t)((((uint32_t)buf[0] << 12) | ((uint32_t)buf[1] << 4) | (buf[2] >> 4)));
    int32_t adc_T = (int32_t)((((uint32_t)buf[3] << 12) | ((uint32_t)buf[4] << 4) | (buf[5] >> 4)));

    // Compensar temperatura
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)dev->calib.dig_T1 << 1))) * ((int32_t)dev->calib.dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)dev->calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)dev->calib.dig_T1))) >> 12) *
                    ((int32_t)dev->calib.dig_T3)) >> 14;
    dev->t_fine = var1 + var2;
    data->temperature = (dev->t_fine * 5 + 128) >> 8; // Centésimas de grado

    // Compensar presión
    int64_t var1_p, var2_p, p;
    var1_p = ((int64_t)dev->t_fine) - 128000;
    var2_p = var1_p * var1_p * (int64_t)dev->calib.dig_P6;
    var2_p = var2_p + ((var1_p * (int64_t)dev->calib.dig_P5) << 17);
    var2_p = var2_p + (((int64_t)dev->calib.dig_P4) << 35);
    var1_p = ((var1_p * var1_p * (int64_t)dev->calib.dig_P3) >> 8) +
             ((var1_p * (int64_t)dev->calib.dig_P2) << 12);
    var1_p = (((((int64_t)1) << 47) + var1_p)) * ((int64_t)dev->calib.dig_P1) >> 33;

    if (var1_p == 0)
    {
        data->pressure = 0;
    }
    else
    {
        p = 1048576 - adc_P;
        p = (((p << 31) - var2_p) * 3125) / var1_p;
        var1_p = (((int64_t)dev->calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
        var2_p = (((int64_t)dev->calib.dig_P8) * p) >> 19;
        p = ((p + var1_p + var2_p) >> 8) + (((int64_t)dev->calib.dig_P7) << 4);
        data->pressure = (uint32_t)(p >> 8); // en Pa
    }
}

uint8_t BMP280_ReadID(BMP280_t *dev)
{
    uint8_t id = 0;
    BMP280_ReadRegs(dev, BMP280_REG_ID, &id, 1);
    return id;
}

void BMP280_ReadRegs(BMP280_t *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(dev->hspi, &reg, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(dev->hspi, buf, len, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
}

void BMP280_WriteReg(BMP280_t *dev, uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg & 0x7F, value}; // bit 7 clear para escritura
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(dev->hspi, buf, 2, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
}
