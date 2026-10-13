#pragma once
// -----------------------------------------------------------------------------
// Public C API for the EOM_Controller_NI DLL.
//
// This plain-C interface lets the DLL be used from other languages (e.g. the
// eom_controller Python package via ctypes). When building the DLL, the CMake target
// defines EOMCONTROLLERNI_EXPORTS so the symbols are exported; consumers that
// include this header without that define get dllimport declarations.
// -----------------------------------------------------------------------------

#if defined(_WIN32)
    #if defined(EOMCONTROLLERNI_EXPORTS)
        #define EOM_API __declspec(dllexport)
    #else
        #define EOM_API __declspec(dllimport)
    #endif
#else
    #define EOM_API
#endif

#if !defined(__cplusplus)
    #include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to an EOM_Regulator instance.
typedef void* EOM_HANDLE;

// Create/initialize an instance. Returns NULL on failure.
EOM_API EOM_HANDLE EOM_Create(
    short board_idx,
    double dac_min, double dac_max,
    double adc_min, double adc_max,
    double min_threshold,
    double max_threshold);

// Start the regulation loop (spawns the worker thread). Returns false on failure
// or if handle is NULL.
EOM_API bool EOM_Start(EOM_HANDLE handle);

// Stop the regulation loop and join the worker thread.
EOM_API void EOM_Stop(EOM_HANDLE handle);

// Read the latest diode value, bias value and laser state. Output pointers may
// be NULL individually.
EOM_API void EOM_GetValue(EOM_HANDLE handle, double* diode_out, double* bias_out, bool* laser_on_out);

// Stop (if needed) and destroy the instance, freeing its memory.
EOM_API void EOM_Destroy(EOM_HANDLE handle);

// -----------------------------------------------------------------------------
// Dual controller API.
//
// Regulates TWO EOMs at once from a single NI device, on one worker thread (no
// concurrent access to the NI box). Channel mapping is fixed:
//     EOM 1 -> AO0 / AI0
//     EOM 2 -> AO1 / AI1
// Each EOM has its own DAC/ADC ranges and power thresholds.
// -----------------------------------------------------------------------------

// Opaque handle to a DualEOMController instance.
typedef void* EOM2_HANDLE;

// Create/initialize a dual instance. Returns NULL on failure (e.g. a channel
// could not be opened on the device).
EOM_API EOM2_HANDLE EOM2_Create(
    short board_idx,
    // EOM 1 (AO0 / AI0)
    double dac1_min, double dac1_max,
    double adc1_min, double adc1_max,
    double min1_threshold, double max1_threshold,
    // EOM 2 (AO1 / AI1)
    double dac2_min, double dac2_max,
    double adc2_min, double adc2_max,
    double min2_threshold, double max2_threshold);

// Start the (single) regulation thread that services both EOMs. Returns false
// on failure or if handle is NULL.
EOM_API bool EOM2_Start(EOM2_HANDLE handle);

// Stop the regulation thread and join it.
EOM_API void EOM2_Stop(EOM2_HANDLE handle);

// Read the latest diode/bias/laser state for both EOMs. Any output pointer may
// be NULL individually.
EOM_API void EOM2_GetValues(EOM2_HANDLE handle,
    double* diode1_out, double* bias1_out, bool* laser1_on_out,
    double* diode2_out, double* bias2_out, bool* laser2_on_out);

// Stop (if needed) and destroy the instance, freeing its memory.
EOM_API void EOM2_Destroy(EOM2_HANDLE handle);

#ifdef __cplusplus
} // extern "C"
#endif
