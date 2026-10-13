#pragma once
#ifndef DAC_MODULE_H_INCLUDED
#define DAC_MODULE_H_INCLUDED

#include "NIDAQmx.h"
#include <thread>

class EOM_Regulator {

	// NI-DAQ Handles
    char errBuff[2048] = { '\0' };
    TaskHandle eom_bias = 0;
    TaskHandle pd = 0;
    short board_idx = 1;
    float dac_timeout = 1.0;
    unsigned short error = 0;

    // User defined Settings
    double range_DAC[2];
    double range_ADC[2];
    double min_power, max_power;

    // Reg values
    double minSensorValue = 0.0;
    double min_voltage = 0.0;
    double diode_value = 0.0;

    // Stage Flags and main thread
    std::thread thr;
    bool run_regulation = false, laser_on = false;
    

public:

    EOM_Regulator(short idx, double dac_min, double dac_max, double adc_min, double adc_max, double min_thrshold, double max_threshold);

    void get_values(double* diode, double* bias, bool* laser);

    void regulateWindow(double window, double step_size);

    void init_regulation();

    void loop();

    bool start();

    void stop();

    void regulation_mode();
};



#endif