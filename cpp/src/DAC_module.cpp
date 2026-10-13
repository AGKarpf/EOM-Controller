#include "pch.h"
#include "DAC_module.h"
#include "DAC_functions.h"
#include <chrono>
#include <iostream>

const unsigned short ERROR_NONE = 0x0000;
const unsigned short ERROR_disconnected = 0x0001; // 0000 0000 0000 0001
const unsigned short ERROR_LOW_POWER = 0x0002; // 0000 0000 0000 0010

EOM_Regulator::EOM_Regulator(short idx, double dac_min, double dac_max, double adc_min, double adc_max, double min_thrshold, double max_threshold) {
    board_idx = idx;
    min_power = min_thrshold;
    max_power = max_threshold;
    range_DAC[0] = dac_min;
    range_DAC[1] = dac_max;
    range_ADC[0] = adc_min;
    range_ADC[1] = adc_max;
    
    laser_on = false;
    if (!create_analog_output(&eom_bias, board_idx, 1, range_DAC)) { eom_bias = 0; }
    if (!create_analog_input(&pd, board_idx, 0, range_ADC)) { 
        std::cout << "PD-ERROR\n";
        pd = 0; }

    std::cout << pd << " :: " << eom_bias << "\n";
}

void EOM_Regulator::get_values(double* diode, double* bias, bool* laser) {
    *diode = diode_value;
    *laser = laser_on;
	*bias = min_voltage;
}

void EOM_Regulator::regulateWindow(double window, double step_size) {
    double minValue = read_avr_value(&pd, 100, 20000);
    double best_Voltage = min_voltage;      // last measured min Voltage
	// Do a scan around the last minimum
    double start = min_voltage - window;
    double end = min_voltage + window;
    if (start < range_DAC[0]) {
        start = range_DAC[0];
    }
    if (end > range_DAC[1]) {
        end = range_DAC[1];
    }
    
    std::cout << "Run Window\n";
    for (double voltage = start; voltage < end; voltage += step_size) {
        DAQmxWriteAnalogScalarF64(eom_bias, true, dac_timeout, voltage, nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        double current_voltage = read_avr_value(&pd, 100, 20000);
        if (current_voltage < minValue) {
            minValue = current_voltage;
            best_Voltage = voltage;
        }
    }

    min_voltage = best_Voltage;
    DAQmxWriteAnalogScalarF64(eom_bias, true, dac_timeout, best_Voltage, nullptr);

    minSensorValue = read_avr_value(&pd, 100, 20000);
    diode_value = minSensorValue;
}

void EOM_Regulator::init_regulation() {
    /*if (range_DAC[1] - range_DAC[0] > 5) {
        regulateWindow(20, 0.2);
    }
    else if (range_DAC[1] - range_DAC[0] > 3) {
        regulateWindow(3, 0.01);
    }
    */
    regulateWindow(20, 0.2);
    regulateWindow(0.4, 0.01);
    //regulateWindow(0.05, 0.001);
}

void EOM_Regulator::loop() {
    int loops_run = 0;
    long long miliseconds_to_wait = 200;
    int loops_to_sleep = 120;
    run_regulation = true;
    std::cout << "Starting LOOP: " << run_regulation << "\n";
    std::cout << "DAC Range: " << range_DAC[0] << ", " << range_DAC[1] << "\n";
    std::cout << "ADC Range: " << range_ADC[0] << ", " << range_ADC[1] << "\n";
    
    while (run_regulation) {
        diode_value = read_avr_value(&pd, 100, 20000);
        //std::cout << "Check for PD: " << diode_value << "\n";
        if (diode_value  > min_power) {
            init_regulation();
            laser_on = true;
            std::cout << "Starting Reg: " << diode_value << "\n";
            regulation_mode();
            laser_on = false;
            loops_run = 0;
            miliseconds_to_wait = 200;
        }
        else {
            //std::cout << "Set Values\n";
            laser_on = false;
            
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(miliseconds_to_wait));
        
        loops_run++;
        //std::cout << "Check for LOOPS: " << loops_run << "\n";
        if (loops_run == loops_to_sleep) {
            
            miliseconds_to_wait = 2000;
            DAQmxWriteAnalogScalarF64(eom_bias, true, dac_timeout, 0.0, nullptr);
            min_voltage = 0.0;
        }
    }
}

void EOM_Regulator::regulation_mode() {
    
    diode_value = read_avr_value(&pd, 100, 20000);
    while ((diode_value > min_power) && run_regulation) {
        diode_value = read_avr_value(&pd, 100, 20000);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (diode_value > minSensorValue * 1.02 || diode_value < minSensorValue * 0.98) {
            regulateWindow(0.1, 0.01);
        }
        else if (diode_value > max_power) {
            std::cout << "PD to high\n";
            regulateWindow(0.1, 0.01);
        }
    }
}

void EOM_Regulator::stop() {
    run_regulation = false;
    thr.join();
}

bool EOM_Regulator::start() {
    thr = std::thread([&]() { loop(); });
    return true;
}