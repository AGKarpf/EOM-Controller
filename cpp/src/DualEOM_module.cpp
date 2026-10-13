#include "pch.h"
#include "DualEOM_module.h"
#include "DAC_functions.h"

#include <stdexcept>
#include <iostream>

// =============================================================================
// RegChannel
// =============================================================================

RegChannel::RegChannel(short idx, char ao_ch, short ai_ch,
                       double dac_min, double dac_max,
                       double adc_min, double adc_max,
                       double min_threshold, double max_threshold)
{
    board_idx  = idx;
    ao_channel = ao_ch;
    ai_channel = ai_ch;
    min_power  = min_threshold;
    max_power  = max_threshold;
    range_DAC[0] = dac_min;
    range_DAC[1] = dac_max;
    range_ADC[0] = adc_min;
    range_ADC[1] = adc_max;
}

bool RegChannel::init() {
    if (!create_analog_output(&ao_task, board_idx, ao_channel, range_DAC)) {
        ao_task = 0;
        return false;
    }
    if (!create_analog_input(&ai_task, board_idx, ai_channel, range_ADC)) {
        ai_task = 0;
        return false;
    }
    // Start from a known state.
    writeBias(0.0);
    std::cout << "RegChannel init: AI" << static_cast<int>(ai_channel)
              << " / AO" << static_cast<int>(ao_channel)
              << " (ai_task=" << ai_task << ", ao_task=" << ao_task << ")\n";
    return true;
}

double RegChannel::readPD() {
    if (!ai_task) return 0.0;
    // stop_after=true: release the shared AI timing engine for the other channel.
    return read_avr_value(&ai_task, 100, 20000, /*stop_after=*/true);
}

void RegChannel::writeBias(double volts) {
    if (ao_task) {
        DAQmxWriteAnalogScalarF64(ao_task, true, dac_timeout, volts, nullptr);
    }
}

void RegChannel::beginScan(double window, double step, State next) {
    double start = min_voltage - window;
    double end   = min_voltage + window;
    if (start < range_DAC[0]) start = range_DAC[0];
    if (end   > range_DAC[1]) end   = range_DAC[1];

    scan_v     = start;
    scan_end   = end;
    scan_step  = step;
    scan_min   = readPD();       // baseline (mirrors the single-EOM regulateWindow)
    scan_best  = min_voltage;
    after_scan = next;
    state      = State::Scanning;
}

bool RegChannel::stepScan() {
    if (scan_v < scan_end) {
        writeBias(scan_v);
        // Short settle only -- this is the atomic unit that briefly blocks the
        // other channel (~a few ms), which is acceptable and keeps things simple.
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        double v = readPD();
        if (v < scan_min) {
            scan_min  = v;
            scan_best = scan_v;
        }
        scan_v += scan_step;
        return false;   // more points to go
    }

    // Scan finished: settle on the best voltage and publish the diode value.
    min_voltage    = scan_best;
    writeBias(scan_best);
    minSensorValue = readPD();
    diode_value    = minSensorValue;
    return true;
}

void RegChannel::step() {
    switch (state) {

    case State::Idle: {
        auto now = clock::now();
        if (now < next_idle_read) return;                 // still "waiting" (non-blocking)
        next_idle_read = now + std::chrono::milliseconds(idle_interval_ms);

        diode_value = readPD();
        if (diode_value > min_power) {
            // Laser present -> run initial regulation: coarse pass then fine pass,
            // then hand over to tracking.
            laser_on         = true;
            parked           = false;
            idle_loops       = 0;
            idle_interval_ms = 200;
            pending_passes   = { { 20.0, 0.2 }, { 0.4, 0.01 } };
            auto pass = pending_passes.front();
            pending_passes.erase(pending_passes.begin());
            beginScan(pass.first, pass.second, State::Tracking);
        }
        else {
            laser_on = false;
            ++idle_loops;
            // After a while with no laser, park the bias at 0 and poll more slowly.
            if (idle_loops >= 120 && !parked) {
                writeBias(0.0);
                min_voltage      = 0.0;
                idle_interval_ms = 2000;
                parked           = true;
            }
        }
        return;
    }

    case State::Scanning: {
        if (stepScan()) {
            if (!pending_passes.empty()) {
                auto pass = pending_passes.front();
                pending_passes.erase(pending_passes.begin());
                beginScan(pass.first, pass.second, after_scan);
            }
            else {
                state = after_scan;
                if (state == State::Tracking) {
                    next_track_read = clock::now() + std::chrono::milliseconds(100);
                }
            }
        }
        return;
    }

    case State::Tracking: {
        auto now = clock::now();
        if (now < next_track_read) return;                // hold the tracking interval
        next_track_read = now + std::chrono::milliseconds(100);

        diode_value = readPD();
        if (diode_value <= min_power) {
            // Laser lost -> return to idle polling.
            laser_on         = false;
            idle_loops       = 0;
            idle_interval_ms = 200;
            parked           = false;
            next_idle_read   = now;
            state            = State::Idle;
            return;
        }

        // Re-optimize if the diode drifted off the minimum or climbed too high.
        if (diode_value > minSensorValue * 1.02 ||
            diode_value < minSensorValue * 0.98 ||
            diode_value > max_power) {
            beginScan(0.1, 0.01, State::Tracking);
        }
        return;
    }
    }
}

void RegChannel::park() {
    writeBias(0.0);
}

void RegChannel::shutdown() {
    if (ao_task) {
        writeBias(0.0);
        DAQmxStopTask(ao_task);
        DAQmxClearTask(ao_task);
        ao_task = 0;
    }
    if (ai_task) {
        DAQmxStopTask(ai_task);
        DAQmxClearTask(ai_task);
        ai_task = 0;
    }
}

void RegChannel::get_values(double* diode, double* bias, bool* laser) const {
    if (diode) *diode = diode_value;
    if (bias)  *bias  = min_voltage;
    if (laser) *laser = laser_on;
}

// =============================================================================
// DualEOMController
// =============================================================================

DualEOMController::DualEOMController(short board_idx,
    double dac1_min, double dac1_max, double adc1_min, double adc1_max,
    double min1_threshold, double max1_threshold,
    double dac2_min, double dac2_max, double adc2_min, double adc2_max,
    double min2_threshold, double max2_threshold)
    : ch1(board_idx, /*ao*/0, /*ai*/0,
          dac1_min, dac1_max, adc1_min, adc1_max, min1_threshold, max1_threshold),
      ch2(board_idx, /*ao*/1, /*ai*/1,
          dac2_min, dac2_max, adc2_min, adc2_max, min2_threshold, max2_threshold)
{
    if (!ch1.init()) throw std::runtime_error("EOM 1 (AI0/AO0) initialization failed");
    if (!ch2.init()) throw std::runtime_error("EOM 2 (AI1/AO1) initialization failed");
}

DualEOMController::~DualEOMController() {
    stop();
    ch1.shutdown();
    ch2.shutdown();
}

bool DualEOMController::start() {
    if (run_regulation) return true;          // already running
    run_regulation = true;
    thr = std::thread([this]() { loop(); });
    return true;
}

void DualEOMController::loop() {
    std::cout << "DualEOMController loop started (single thread, both EOMs)\n";
    while (run_regulation) {
        // Round-robin: one cooperative step per channel per tick. Each step is
        // short and non-blocking, so both EOMs are effectively regulated at once
        // while only this thread ever talks to the NI box.
        ch1.step();
        ch2.step();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // Safe state on exit.
    ch1.park();
    ch2.park();
}

void DualEOMController::stop() {
    if (run_regulation) {
        run_regulation = false;
        if (thr.joinable()) thr.join();
    }
}

void DualEOMController::get_values(double* diode1, double* bias1, bool* laser1,
                                   double* diode2, double* bias2, bool* laser2) const {
    ch1.get_values(diode1, bias1, laser1);
    ch2.get_values(diode2, bias2, laser2);
}
