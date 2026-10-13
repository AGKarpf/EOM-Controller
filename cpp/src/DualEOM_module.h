#pragma once
#ifndef DUAL_EOM_MODULE_H_INCLUDED
#define DUAL_EOM_MODULE_H_INCLUDED

#include "NIDAQmx.h"
#include <thread>
#include <atomic>
#include <vector>
#include <utility>
#include <chrono>

// -----------------------------------------------------------------------------
// RegChannel: one EOM regulation channel (one AO + one AI on the shared NI box).
//
// Unlike the single-EOM EOM_Regulator, the regulation logic here is a
// *non-blocking* cooperative state machine. Each call to step() performs at most
// one short measurement / one voltage step and then returns. That lets a single
// worker thread service several channels by round-robin (see DualEOMController)
// without any channel starving the others -- and, crucially, without ever
// touching NI-DAQmx from more than one thread. Long waits (idle polling, the
// tracking interval) are handled via steady_clock deadlines instead of blocking
// sleeps, so the other channel keeps running while this one "waits".
// -----------------------------------------------------------------------------
class RegChannel {
public:
    RegChannel(short board_idx, char ao_channel, short ai_channel,
               double dac_min, double dac_max,
               double adc_min, double adc_max,
               double min_threshold, double max_threshold);

    bool init();                 // create the DAQmx tasks; false on failure
    void step();                 // advance the state machine by one unit of work
    void park();                 // drive the bias to 0 V (safe idle state)
    void shutdown();             // stop + clear the DAQmx tasks

    void get_values(double* diode, double* bias, bool* laser) const;

private:
    enum class State { Idle, Scanning, Tracking };
    using clock = std::chrono::steady_clock;

    double readPD();                                  // one finite AI read
    void   writeBias(double volts);                   // one on-demand AO write
    void   beginScan(double window, double step, State next);
    bool   stepScan();                                // one scan point; true = done

    // Hardware / identity
    TaskHandle ao_task = 0;
    TaskHandle ai_task = 0;
    short board_idx = 1;
    char  ao_channel = 0;
    short ai_channel = 0;
    float dac_timeout = 1.0f;

    // User settings
    double range_DAC[2];
    double range_ADC[2];
    double min_power, max_power;

    // Regulation values (published via get_values)
    double min_voltage = 0.0;
    double minSensorValue = 0.0;
    double diode_value = 0.0;
    bool   laser_on = false;

    // State machine
    State state = State::Idle;
    State after_scan = State::Tracking;

    // Idle-polling timing
    clock::time_point next_idle_read = clock::now();
    int  idle_loops = 0;
    long idle_interval_ms = 200;
    bool parked = false;

    // Tracking timing
    clock::time_point next_track_read = clock::now();

    // Scan progress
    std::vector<std::pair<double, double>> pending_passes; // (window, step)
    double scan_v = 0.0, scan_end = 0.0, scan_step = 0.0;
    double scan_min = 0.0, scan_best = 0.0;
};

// -----------------------------------------------------------------------------
// DualEOMController: owns two RegChannels and the single worker thread that
// services both. Only this one thread ever calls into NI-DAQmx, which both
// satisfies the "no concurrent access to the NI box" requirement and matches the
// USB-6001 hardware (it has a single AI timing engine, so its two AI channels
// must be read sequentially anyway).
//
//   EOM 1 -> AO0 / AI0
//   EOM 2 -> AO1 / AI1
// -----------------------------------------------------------------------------
class DualEOMController {
public:
    DualEOMController(short board_idx,
        double dac1_min, double dac1_max, double adc1_min, double adc1_max,
        double min1_threshold, double max1_threshold,
        double dac2_min, double dac2_max, double adc2_min, double adc2_max,
        double min2_threshold, double max2_threshold);
    ~DualEOMController();

    bool start();
    void stop();
    void get_values(double* diode1, double* bias1, bool* laser1,
                    double* diode2, double* bias2, bool* laser2) const;

private:
    void loop();

    RegChannel ch1;   // EOM 1: AO0 / AI0
    RegChannel ch2;   // EOM 2: AO1 / AI1
    std::thread thr;
    std::atomic<bool> run_regulation{ false };
};

#endif // DUAL_EOM_MODULE_H_INCLUDED
