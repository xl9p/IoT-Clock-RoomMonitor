/**
 * Copyright (C) 2024 Bosch Sensortec GmbH
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>

#include "bme69x.h"
#include "common.h"
#include "coines.h"

/***********************************************************************/
/*                         Macros                                      */
/***********************************************************************/

/*
 * Macro definition for valid new data (0x80) AND
 * heater stability (0x10) AND gas resistance (0x20) values
 */
#define BME69X_VALID_DATA  UINT8_C(0xB0)

/* Macro for count of samples to be displayed */
#define SAMPLE_COUNT       UINT8_C(50)

/***********************************************************************/
/*                         Test code                                   */
/***********************************************************************/

int main(void)
{
    struct bme69x_dev bme;
    int8_t rslt;
    struct bme69x_conf conf;
    struct bme69x_heatr_conf heatr_conf;
    struct bme69x_data data[3];
    uint32_t del_period;
    uint8_t n_fields;
    uint32_t time_ms = 0;
    uint16_t sample_count = 1;

    /* Heater temperature in degree Celsius */
    uint16_t temp_prof[10] = { 320, 100, 100, 100, 200, 200, 200, 320, 320, 320 };

    /* Multiplier to the shared heater duration */
    uint16_t mul_prof[10] = { 5, 2, 10, 30, 5, 5, 5, 5, 5, 5 };

    /* Interface preference is updated as a parameter
     * For I2C : BME69X_I2C_INTF
     * For SPI : BME69X_SPI_INTF
     */
    rslt = bme69x_interface_init(&bme, BME69X_SPI_INTF);
    bme69x_check_rslt("bme69x_interface_init", rslt);

    rslt = bme69x_init(&bme);
    bme69x_check_rslt("bme69x_init", rslt);

    /* Check if rslt == BME69X_OK, report or handle if otherwise */
    rslt = bme69x_get_conf(&conf, &bme);
    bme69x_check_rslt("bme69x_get_conf", rslt);

    /* Check if rslt == BME69X_OK, report or handle if otherwise */
    conf.filter = BME69X_FILTER_OFF;
    conf.odr = BME69X_ODR_NONE;
    conf.os_hum = BME69X_OS_1X;
    conf.os_pres = BME69X_OS_16X;
    conf.os_temp = BME69X_OS_2X;
    rslt = bme69x_set_conf(&conf, &bme);
    bme69x_check_rslt("bme69x_set_conf", rslt);

    /* Check if rslt == BME69X_OK, report or handle if otherwise */
    heatr_conf.enable = BME69X_ENABLE;
    heatr_conf.heatr_temp_prof = temp_prof;
    heatr_conf.heatr_dur_prof = mul_prof;

    /* Shared heating duration in milliseconds */
    heatr_conf.shared_heatr_dur = (uint16_t)(140 - (bme69x_get_meas_dur(BME69X_PARALLEL_MODE, &conf, &bme) / 1000));

    heatr_conf.profile_len = 10;
    rslt = bme69x_set_heatr_conf(BME69X_PARALLEL_MODE, &heatr_conf, &bme);
    bme69x_check_rslt("bme69x_set_heatr_conf", rslt);

    /* Check if rslt == BME69X_OK, report or handle if otherwise */
    rslt = bme69x_set_op_mode(BME69X_PARALLEL_MODE, &bme);
    bme69x_check_rslt("bme69x_set_op_mode", rslt);

    printf(
        "Print parallel mode data if mask for new data(0x80), gas measurement(0x20) and heater stability(0x10) are set\n\n");

    /* Check if rslt == BME69X_OK, report or handle if otherwise */
    printf(
        "Sample, TimeStamp(ms), Temperature(deg C), Pressure(Pa), Humidity(%%), Gas resistance(ohm), Status, Gas index, Meas index\n");
    while (sample_count <= SAMPLE_COUNT)
    {
        /* Calculate delay period in microseconds */
        del_period = bme69x_get_meas_dur(BME69X_PARALLEL_MODE, &conf, &bme) + (heatr_conf.shared_heatr_dur * 1000);
        bme.delay_us(del_period, bme.intf_ptr);

        time_ms = coines_get_millis();

        rslt = bme69x_get_data(BME69X_PARALLEL_MODE, data, &n_fields, &bme);
        bme69x_check_rslt("bme69x_get_data", rslt);

        /* Check if rslt == BME69X_OK, report or handle if otherwise */
        for (uint8_t i = 0; i < n_fields; i++)
        {
            if (data[i].status == BME69X_VALID_DATA)
            {
#ifdef BME69X_USE_FPU
                printf("%u, %lu, %.2f, %.2f, %.2f, %.2f, 0x%x, %d, %d\n",
                       sample_count,
                       (long unsigned int)time_ms,
                       data[i].temperature,
                       data[i].pressure,
                       data[i].humidity,
                       data[i].gas_resistance,
                       data[i].status,
                       data[i].gas_index,
                       data[i].meas_index);
#else
                printf("%u, %lu, %d, %lu, %lu, %lu, 0x%x, %d, %d\n",
                       sample_count,
                       (long unsigned int)time_ms,
                       (data[i].temperature),
                       (long unsigned int)data[i].pressure,
                       (long unsigned int)(data[i].humidity),
                       (long unsigned int)data[i].gas_resistance,
                       data[i].status,
                       data[i].gas_index,
                       data[i].meas_index);
#endif
                sample_count++;
            }
        }
    }

    bme69x_coines_deinit();

    return 0;
}
