# EOM-Controller

Bias regulation for **two EOMs** on one **NI USB-6001** (NI-DAQmx):

* `cpp/` – C++ source of `EOM_Controller_NI.dll` (single `EOM_*` and dual `EOM2_*` API)
* `eom_controller/` – Python package: ctypes wrapper, per-laser channel views,
  hardware-free mock, PyQt6 widgets and a standalone GUI. A prebuilt x64
  `EOM_Controller_NI.dll` is bundled in `eom_controller/bin/`.

This repository merges the former `EOM-Controller-NI` (C++) and `EOM-GUI` (Python) projects.

## Installation

```bash
pip install "eom-controller[gui] @ git+https://github.com/AGKarpf/EOM-Controller.git"
```

Development checkout: `pip install -e .[gui,test]` (or run `setup_env.bat`).

Hardware operation needs Windows, the **NI-DAQmx runtime** and an NI USB-6001.
The mock (`MockDualEOMController`) runs anywhere.

## Python usage

```python
import eom_controller                   # import BEFORE PyQt6 (see "Qt and the DLL")
from eom_controller import DualEOMController, SharedEom, EomChannel

ctrl = DualEOMController.from_config_path("eom.ini")
shared = SharedEom(ctrl)                # one controller, one worker thread
eom1, eom2 = EomChannel(shared, 0), EomChannel(shared, 1)
shared.start()                          # start/stop always affect BOTH channels
diode, bias, seed_ok = eom1.value()
```

Widgets for your own GUIs:

```python
from eom_controller.gui import EomWidget, DualEomWindow
widget = EomWidget(eom1, title="Seed indicator")   # green = seed power OK
```

Standalone GUI:

```bash
py -m eom_controller              # uses ./eom.ini, else the packaged default
py -m eom_controller --ini my.ini
py -m eom_controller --mock       # no hardware
```

After `pip install`, `eom-controller-gui` starts the same GUI without a console.

### Qt and the DLL

The DLL and the NI-DAQmx runtime must be loaded **before Qt**: Qt ships its own
Visual C++ runtime, and if it loads first the DLL crashes inside `EOM_Create`.
`py -m eom_controller` handles this; in your own application call
`eom_controller.preload_dll()` (or create the controller) before importing PyQt6.

## Configuration (`eom.ini`)

Template: [`eom_controller/default_eom.ini`](eom_controller/default_eom.ini).
One section per EOM, `[EOM1]` and `[EOM2]` (legacy `[EOM]` accepted for EOM 1):

| Key | Unit | Meaning |
| --- | --- | --- |
| `board_idx` | – | NI device index, `Dev<board_idx>` (read from `[EOM1]`, shared device) |
| `dac_min` / `dac_max` | V | Analog-output range = allowed EOM bias (`AO0` EOM 1, `AO1` EOM 2) |
| `adc_min` / `adc_max` | V | Analog-input range for the photodiode (`AI0` EOM 1, `AI1` EOM 2) |
| `min_threshold` | V | Photodiode level above which the laser counts as **on** and regulation runs |
| `max_threshold` | V | Above this level the regulator re-scans for the power minimum |

`[EOM_DLL]`: `dll_path` (empty = bundled DLL; relative paths are relative to the
INI file) and `use_stdcall` (`False` = cdecl, matches the DLL).

## Hardware wiring

* **EOM 1:** `Dev1/ao0` → EOM 1 bias; photodiode 1 → `Dev1/ai0` (RSE)
* **EOM 2:** `Dev1/ao1` → EOM 2 bias; photodiode 2 → `Dev1/ai1` (RSE)

The regulator drives each bias output to **minimize** its photodiode signal.
The USB-6001 has a single AI timing engine, so the dual controller reads both
inputs sequentially on one worker thread (cooperative state machine).

The single-EOM API uses `AO1`/`AI0` and is kept for backward compatibility.

## Building the DLL (`cpp/`)

Requirements: MSVC (VS 2022/2026 or Build Tools), CMake ≥ 3.20 (≥ 4.2 for the
VS 2026 preset), NI-DAQmx with C/ANSI development support.

```bash
cd cpp
cmake --preset release
cmake --build --preset release
```

The DLL lands in `cpp/build/bin/Release/` and is copied automatically into
`eom_controller/bin/` (disable with `-DEOM_COPY_TO_PYTHON=OFF`). Without presets:
`cmake -B build -A x64` and `cmake --build build --config Release`. Non-standard
NI-DAQmx location: `-DNIDAQMX_ROOT="C:/path/to/.../C"`.

### Exported C API

```c
void* EOM2_Create(short board_idx,
                  double dac1_min, double dac1_max, double adc1_min, double adc1_max,
                  double min1_threshold, double max1_threshold,
                  double dac2_min, double dac2_max, double adc2_min, double adc2_max,
                  double min2_threshold, double max2_threshold);
bool  EOM2_Start(void*);
void  EOM2_Stop(void*);
void  EOM2_GetValues(void*, double* diode1, double* bias1, bool* laser1_on,
                            double* diode2, double* bias2, bool* laser2_on);
void  EOM2_Destroy(void*);

/* single EOM: EOM_Create / EOM_Start / EOM_Stop / EOM_GetValue / EOM_Destroy */
```

## Tests

```bash
py -m pytest tests
```
