// dllmain.cpp
// Exposes a plain C API for EOM_Regulator so it can be used from other
// languages (e.g. the eom_controller Python package via ctypes).
//
// The public signatures live in include/EOM_Controller_NI.h; this file provides
// the definitions. Building with EOMCONTROLLERNI_EXPORTS defined (see
// CMakeLists.txt) exports the symbols from the DLL.

#include "pch.h"
#include "EOM_Controller_NI.h"   // public C API (EOM_API, EOM_HANDLE, prototypes)
#include "DAC_module.h"          // EOM_Regulator (single EOM)
#include "DualEOM_module.h"      // DualEOMController (two EOMs, single thread)

extern "C" {

    EOM_API EOM_HANDLE EOM_Create(
        short board_idx,
        double dac_min, double dac_max,
        double adc_min, double adc_max,
        double min_threshold,
        double max_threshold)
    {
        try {
            auto* obj = new EOM_Regulator(
                board_idx,
                dac_min, dac_max,
                adc_min, adc_max,
                min_threshold,
                max_threshold
            );
            return static_cast<EOM_HANDLE>(obj);
        }
        catch (...) {
            return nullptr;
        }
    }

    EOM_API bool EOM_Start(EOM_HANDLE handle)
    {
        if (!handle) return false;
        auto* obj = static_cast<EOM_Regulator*>(handle);
        try {
            return obj->start();
        }
        catch (...) {
            return false;
        }
    }

    EOM_API void EOM_Stop(EOM_HANDLE handle)
    {
        if (!handle) return;
        auto* obj = static_cast<EOM_Regulator*>(handle);
        try {
            obj->stop();
        }
        catch (...) {
            // swallow exceptions to keep the C ABI stable
        }
    }

    EOM_API void EOM_GetValue(EOM_HANDLE handle, double* diode_out, double* bias_out, bool* laser_on_out)
    {
        if (!handle) return;
        auto* obj = static_cast<EOM_Regulator*>(handle);

        double diode = 0.0;
        double bias = 0.0;
        bool laser = false;

        try {
            obj->get_values(&diode, &bias, &laser);
        }
        catch (...) {
            // On error, leave defaults (0.0 / false)
        }

        if (diode_out)    *diode_out = diode;
        if (bias_out)     *bias_out = bias;   // (was guarded by diode_out in the old code)
        if (laser_on_out) *laser_on_out = laser;
    }

    EOM_API void EOM_Destroy(EOM_HANDLE handle)
    {
        if (!handle) return;
        auto* obj = static_cast<EOM_Regulator*>(handle);
        try {
            // Ensure the background thread is stopped before delete
            obj->stop();
        }
        catch (...) {
            // ignore
        }
        delete obj;
    }

    // -------------------------------------------------------------------------
    // Dual controller (two EOMs, single worker thread)
    // -------------------------------------------------------------------------

    EOM_API EOM2_HANDLE EOM2_Create(
        short board_idx,
        double dac1_min, double dac1_max,
        double adc1_min, double adc1_max,
        double min1_threshold, double max1_threshold,
        double dac2_min, double dac2_max,
        double adc2_min, double adc2_max,
        double min2_threshold, double max2_threshold)
    {
        try {
            auto* obj = new DualEOMController(
                board_idx,
                dac1_min, dac1_max, adc1_min, adc1_max, min1_threshold, max1_threshold,
                dac2_min, dac2_max, adc2_min, adc2_max, min2_threshold, max2_threshold
            );
            return static_cast<EOM2_HANDLE>(obj);
        }
        catch (...) {
            return nullptr;
        }
    }

    EOM_API bool EOM2_Start(EOM2_HANDLE handle)
    {
        if (!handle) return false;
        auto* obj = static_cast<DualEOMController*>(handle);
        try {
            return obj->start();
        }
        catch (...) {
            return false;
        }
    }

    EOM_API void EOM2_Stop(EOM2_HANDLE handle)
    {
        if (!handle) return;
        auto* obj = static_cast<DualEOMController*>(handle);
        try {
            obj->stop();
        }
        catch (...) {
            // swallow exceptions to keep the C ABI stable
        }
    }

    EOM_API void EOM2_GetValues(EOM2_HANDLE handle,
        double* diode1_out, double* bias1_out, bool* laser1_on_out,
        double* diode2_out, double* bias2_out, bool* laser2_on_out)
    {
        if (!handle) return;
        auto* obj = static_cast<DualEOMController*>(handle);

        double diode1 = 0.0, bias1 = 0.0;
        double diode2 = 0.0, bias2 = 0.0;
        bool   laser1 = false, laser2 = false;

        try {
            obj->get_values(&diode1, &bias1, &laser1, &diode2, &bias2, &laser2);
        }
        catch (...) {
            // On error, leave defaults (0.0 / false)
        }

        if (diode1_out)    *diode1_out = diode1;
        if (bias1_out)     *bias1_out = bias1;
        if (laser1_on_out) *laser1_on_out = laser1;
        if (diode2_out)    *diode2_out = diode2;
        if (bias2_out)     *bias2_out = bias2;
        if (laser2_on_out) *laser2_on_out = laser2;
    }

    EOM_API void EOM2_Destroy(EOM2_HANDLE handle)
    {
        if (!handle) return;
        auto* obj = static_cast<DualEOMController*>(handle);
        try {
            obj->stop();   // (destructor also stops + releases hardware)
        }
        catch (...) {
            // ignore
        }
        delete obj;
    }

} // extern "C"
