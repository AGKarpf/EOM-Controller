#include "pch.h"
#include "DAC_functions.h"
#include <stdexcept>
#include <vector>
#include <stdio.h>
#include <iostream>

bool create_analog_output(TaskHandle* taskHandle,short board, char channel, double range[2]) {
    int32 error = 0;
    char errBuff[2048] = { '\0' };
    char name[20] = { "Dev1/ao0" };
    name[3] = board + 48;
    name[7] = channel + 48;
    std::cout << name << "\n";
   
    // Erstelle einen Task
    error = DAQmxCreateTask("", taskHandle);
    if (error) {
        DAQmxGetExtendedErrorInfo(errBuff, 2048);
        std::cerr << "DAQmx AO CreateTask error (" << name << "): " << errBuff << std::endl;
        return false;
    }
    error = DAQmxCreateAOVoltageChan(*taskHandle,
        name,
        "",
        range[0],         // Minimaler Spannungswert in Volt
        range[1],         // Maximaler Spannungswert in Volt
        DAQmx_Val_Volts,
        nullptr);

    if (error) {
        DAQmxGetExtendedErrorInfo(errBuff, 2048);
        std::cerr << "DAQmx AO channel error (" << name
                  << ", range " << range[0] << ".." << range[1] << " V): "
                  << errBuff << std::endl;
        return false;
    }

    return true;

}

bool create_analog_input(TaskHandle* taskHandle, short board, short channel, double range[2]) {
    int32 error = 0;
    char errBuff[2048] = { '\0' };
    char name[20] = { "Dev1/ai0" };
    name[3] = board + 48;
    name[7] = channel + 48;
    std::cout << name << "\n";
    // Erstelle einen Task
    error = DAQmxCreateTask("", taskHandle);
    if (error) {
        DAQmxGetExtendedErrorInfo(errBuff, 2048);
        std::cerr << "DAQmx AI CreateTask error (" << name << "): " << errBuff << std::endl;
        return false;
    }
    error = DAQmxCreateAIVoltageChan(
        *taskHandle,
        name,                  // Kanalname, z. B. "Dev1/ai0"
        "",                               // Kanalbeschreibung (optional)
        DAQmx_Val_RSE,                   // Referenz (RSE, NRSE, Diff, PseudoDiff)
        range[0],         // Minimaler Spannungswert in Volt
        range[1],         // Maximaler Spannungswert in Volt
        DAQmx_Val_Volts,                 // Einheiten
        nullptr                          // Kalibrierung (optional)
    );
    if (error) {
        DAQmxGetExtendedErrorInfo(errBuff, 2048);
        std::cerr << "DAQmx AI channel error (" << name
                  << ", range " << range[0] << ".." << range[1] << " V): "
                  << errBuff << std::endl;
        return false;
    }
    return true;
}

bool writeAnalogOutput(double value) {
    TaskHandle taskHandle = 0;
    int32 error = 0;
    char errBuff[2048] = { '\0' };

    // Erstelle einen Task
    error = DAQmxCreateTask("", &taskHandle);
    if (error) throw std::runtime_error("Fehler beim Erstellen des Tasks.");

    // F�ge einen analogen Ausgangskanal hinzu (AO0)
    error = DAQmxCreateAOVoltageChan(taskHandle,
        "Dev2/ao0",  // 'Dev1' ist der Standardger�te-Name; passe ihn ggf. an
        "",
        0.0,         // Minimaler Spannungswert in Volt
        5.0,         // Maximaler Spannungswert in Volt
        DAQmx_Val_Volts,
        nullptr);
    if (error) throw std::runtime_error("Fehler beim Hinzuf�gen des analogen Ausgangskanals.");

    // Schreibe den Spannungswert auf den analogen Ausgang
    error = DAQmxWriteAnalogScalarF64(taskHandle, true, 10.0, value, nullptr);
    if (error) throw std::runtime_error("Fehler beim Schreiben auf den analogen Ausgang.");

    
    if (error) {
        return false;
    }
    

    // Task freigeben
    if (taskHandle != 0) {
        DAQmxStopTask(taskHandle);
        DAQmxClearTask(taskHandle);
    }
    return true;
}

double read_avr_value(TaskHandle* taskHandle, int sampleCount, int sampleRate, bool stop_after) {
    std::vector<float64> data(sampleCount);
    float channel_value = 0.0;
    int32 error = 0;
    int32 read = 0;
    error = DAQmxCfgSampClkTiming(
        *taskHandle,
        "",                              // Externer Takt (leer = onboard)
        sampleRate,                      // Abtastrate in Hz
        DAQmx_Val_Rising,                // Abtastung auf steigender Flanke
        DAQmx_Val_FiniteSamps,           // Modus: Endliche Abtastungen
        sampleCount                      // Anzahl der Abtastungen
    );
    error = DAQmxReadAnalogF64(
        *taskHandle,
        sampleCount,                     // Anzahl der zu lesenden Werte
        10.0,                            // Timeout in Sekunden
        DAQmx_Val_GroupByChannel,        // Gruppierung nach Kanal
        data.data(),                     // Zielpuffer
        data.size(),                     // Puffergr��e
        &read,                           // Tats�chlich gelesene Samples
        nullptr                          // Reserviert (immer null)
    );
    for (int i = 0; i < read; ++i) {
        channel_value += data[i];
    }

    // Release the AI timing engine so another AI task on the same device can run.
    if (stop_after) {
        DAQmxStopTask(*taskHandle);
    }

    if (read <= 0) {
        return 0.0;
    }
    double val = channel_value / read;
    //std::cout << "Measured: " << val << "\n";
    return val;
}