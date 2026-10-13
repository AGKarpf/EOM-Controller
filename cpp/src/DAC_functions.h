#pragma once
#ifndef DAC_FUNCTIONS_H_INCLUDED
#define DAC_FUNCTIONS_H_INCLUDED

#include "NIDAQmx.h"

bool create_analog_output(TaskHandle* taskHandle, short board, char channel, double range[2]);

bool create_analog_input(TaskHandle* taskHandle, short board, short channel, double range[2]);

bool writeAnalogOutput(double value);

// Read `sampleCount` finite samples at `sampleRate` and return their mean.
// When `stop_after` is true the task is stopped after the read so the device's
// (single) AI timing engine is released for another AI task -- required when
// more than one AI task shares the same NI device (dual-EOM controller).
double read_avr_value(TaskHandle* taskHandle, int sampleCount, int sampleRate, bool stop_after = false);

#endif