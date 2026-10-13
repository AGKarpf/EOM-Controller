"""ctypes wrapper for EOM_Controller_NI.dll (single EOM_* and dual EOM2_* API).
Parameters come from an INI file ([EOM]/[EOM1]/[EOM2]/[EOM_DLL]). An empty or
missing dll_path selects the DLL bundled with this package; relative paths are
resolved against the INI file's folder."""
from __future__ import annotations
import os
from pathlib import Path
from typing import Tuple, Optional
import ctypes
from ctypes import c_void_p, c_short, c_double, c_bool, POINTER
import configparser

from ._dll import resolve_dll_path

class EOMConfig:
    SECTION = "EOM"
    DLL_SECTION = "EOM_DLL"

    def __init__(
        self,
        board_idx: int,
        dac_min: float,
        dac_max: float,
        adc_min: float,
        adc_max: float,
        min_threshold: float,
        max_threshold: float,
        dll_path: str | None = None,
        use_stdcall: bool = False,
    ):
        self.board_idx = int(board_idx)
        self.dac_min = float(dac_min)
        self.dac_max = float(dac_max)
        self.adc_min = float(adc_min)
        self.adc_max = float(adc_max)
        self.min_threshold = float(min_threshold)
        self.max_threshold = float(max_threshold)
        self.dll_path = dll_path
        self.use_stdcall = bool(use_stdcall)

    @classmethod
    def from_config_file(cls, path: os.PathLike | str) -> "EOMConfig":
        p = Path(path)
        if not p.exists():
            raise FileNotFoundError(f"Config file not found: {p}")
        cfg = configparser.ConfigParser()
        with p.open("r", encoding="utf-8") as f:
            cfg.read_file(f)
        if cls.SECTION not in cfg:
            raise KeyError(f"Missing section [{cls.SECTION}] in {p}")

        s = cfg[cls.SECTION]
        dll_path = str(resolve_dll_path(cfg.get(cls.DLL_SECTION, "dll_path", fallback=""),
                                        base_dir=p.parent))
        use_stdcall = cfg.getboolean(cls.DLL_SECTION, "use_stdcall", fallback=False)

        return cls(
            board_idx=s.getint("board_idx"),
            dac_min=s.getfloat("dac_min"),
            dac_max=s.getfloat("dac_max"),
            adc_min=s.getfloat("adc_min"),
            adc_max=s.getfloat("adc_max"),
            min_threshold=s.getfloat("min_threshold"),
            max_threshold=s.getfloat("max_threshold"),
            dll_path=dll_path,
            use_stdcall=use_stdcall,
        )

    def save(self, path: os.PathLike | str) -> None:
        cfg = configparser.ConfigParser()
        cfg[self.SECTION] = {
            "board_idx": str(self.board_idx),
            "dac_min": repr(self.dac_min),
            "dac_max": repr(self.dac_max),
            "adc_min": repr(self.adc_min),
            "adc_max": repr(self.adc_max),
            "min_threshold": repr(self.min_threshold),
            "max_threshold": repr(self.max_threshold),
        }
        cfg[self.DLL_SECTION] = {
            "dll_path": self.dll_path or "",
            "use_stdcall": "yes" if self.use_stdcall else "no",
        }
        with Path(path).open("w", encoding="utf-8") as f:
            cfg.write(f)


class EOMController:
    def __init__(self, dll_path: os.PathLike | str, config: EOMConfig, use_stdcall: bool = False):
        self._config = config
        self._dll_path = str(dll_path)
        self._lib = self._load_library(self._dll_path, use_stdcall or config.use_stdcall)
        self._bind_signatures()
        self._handle: Optional[int] = None

        self._handle = self._lib.EOM_Create(
            c_short(self._config.board_idx),
            c_double(self._config.dac_min),
            c_double(self._config.dac_max),
            c_double(self._config.adc_min),
            c_double(self._config.adc_max),
            c_double(self._config.min_threshold),
            c_double(self._config.max_threshold),
        )
        if not self._handle:
            raise RuntimeError("EOM_Create returned NULL (failed to create EOM_Regulator instance).")

    @classmethod
    def from_config(cls, dll_path: os.PathLike | str, config_path: os.PathLike | str, use_stdcall: bool = False) -> "EOMController":
        cfg = EOMConfig.from_config_file(config_path)
        return cls(dll_path, cfg, use_stdcall=use_stdcall)

    @classmethod
    def from_config_path(cls, config_path: os.PathLike | str) -> "EOMController":
        cfg = EOMConfig.from_config_file(config_path)
        return cls(cfg.dll_path, cfg, use_stdcall=cfg.use_stdcall)

    @staticmethod
    def _load_library(dll_path: str, use_stdcall: bool = False):
        p = Path(dll_path)
        if not p.exists():
            raise FileNotFoundError(f"DLL not found: {p.resolve()}")
        return ctypes.WinDLL(str(p)) if use_stdcall else ctypes.CDLL(str(p))

    def _bind_signatures(self) -> None:
        # void* EOM_Create(short, double, double, double, double, double, double)
        self._lib.EOM_Create.argtypes = [c_short, c_double, c_double, c_double, c_double, c_double, c_double]
        self._lib.EOM_Create.restype = c_void_p

        # bool EOM_Start(void*)
        self._lib.EOM_Start.argtypes = [c_void_p]
        self._lib.EOM_Start.restype = c_bool

        # void EOM_Stop(void*)
        self._lib.EOM_Stop.argtypes = [c_void_p]
        self._lib.EOM_Stop.restype = None

        # void EOM_GetValue(void*, double*, double*, bool*)
        self._lib.EOM_GetValue.argtypes = [c_void_p, POINTER(c_double), POINTER(c_double), POINTER(c_bool)]
        self._lib.EOM_GetValue.restype = None

        # void EOM_Destroy(void*)
        self._lib.EOM_Destroy.argtypes = [c_void_p]
        self._lib.EOM_Destroy.restype = None

    def start(self) -> bool:
        self._require_handle()
        return bool(self._lib.EOM_Start(self._handle))

    def stop(self) -> None:
        if self._handle:
            self._lib.EOM_Stop(self._handle)

    def get_value(self) -> Tuple[float, float, bool]:
        self._require_handle()
        diode = c_double(0.0)
        bias = c_double(0.0)
        laser = c_bool(False)
        self._lib.EOM_GetValue(self._handle, ctypes.byref(diode), ctypes.byref(bias), ctypes.byref(laser))
        return float(diode.value), float(bias.value), bool(laser.value)

    def close(self) -> None:
        if self._handle:
            try:
                self._lib.EOM_Stop(self._handle)
            except Exception:
                pass
            try:
                self._lib.EOM_Destroy(self._handle)
            finally:
                self._handle = None

    def __enter__(self) -> "EOMController":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def _require_handle(self) -> None:
        if not self._handle:
            raise RuntimeError("EOM controller has been closed or not initialized.")


# =============================================================================
# Dual-EOM support (two EOMs, one NI device, single worker thread in the DLL)
#
#   EOM 1 -> AO0 / AI0
#   EOM 2 -> AO1 / AI1
#
# The DLL exports an independent EOM2_* API; these classes wrap it. Each EOM has
# its own DAC/ADC ranges and thresholds, configured in eom.ini as [EOM1]/[EOM2].
# =============================================================================

# Parameter keys per EOM, in the exact order EOM2_Create expects them.
_EOM_PARAM_KEYS = ("dac_min", "dac_max", "adc_min", "adc_max",
                   "min_threshold", "max_threshold")


class DualEOMConfig:
    EOM1_SECTION = "EOM1"
    EOM2_SECTION = "EOM2"
    LEGACY_SECTION = "EOM"      # accepted as a fallback for EOM 1
    DLL_SECTION = "EOM_DLL"

    def __init__(
        self,
        board_idx: int,
        eom1: Tuple[float, float, float, float, float, float],
        eom2: Tuple[float, float, float, float, float, float],
        dll_path: str | None = None,
        use_stdcall: bool = False,
    ):
        self.board_idx = int(board_idx)
        self.eom1 = tuple(float(x) for x in eom1)
        self.eom2 = tuple(float(x) for x in eom2)
        if len(self.eom1) != 6 or len(self.eom2) != 6:
            raise ValueError("Each EOM needs exactly 6 parameters.")
        self.dll_path = dll_path
        self.use_stdcall = bool(use_stdcall)

    @staticmethod
    def _read_eom_section(cfg: configparser.ConfigParser, section: str) -> Tuple[float, ...]:
        s = cfg[section]
        return tuple(s.getfloat(k) for k in _EOM_PARAM_KEYS)

    @classmethod
    def from_config_file(cls, path: os.PathLike | str) -> "DualEOMConfig":
        p = Path(path)
        if not p.exists():
            raise FileNotFoundError(f"Config file not found: {p}")
        cfg = configparser.ConfigParser()
        with p.open("r", encoding="utf-8") as f:
            cfg.read_file(f)

        # EOM 1: prefer [EOM1], fall back to the legacy single-EOM [EOM] section.
        eom1_section = cls.EOM1_SECTION if cls.EOM1_SECTION in cfg else cls.LEGACY_SECTION
        if eom1_section not in cfg:
            raise KeyError(f"Missing section [{cls.EOM1_SECTION}] (or [{cls.LEGACY_SECTION}]) in {p}")
        if cls.EOM2_SECTION not in cfg:
            raise KeyError(f"Missing section [{cls.EOM2_SECTION}] in {p}")

        board_idx = cfg[eom1_section].getint("board_idx")
        eom1 = cls._read_eom_section(cfg, eom1_section)
        eom2 = cls._read_eom_section(cfg, cls.EOM2_SECTION)

        dll_path = str(resolve_dll_path(cfg.get(cls.DLL_SECTION, "dll_path", fallback=""),
                                        base_dir=p.parent))
        use_stdcall = cfg.getboolean(cls.DLL_SECTION, "use_stdcall", fallback=False)

        return cls(board_idx, eom1, eom2, dll_path=dll_path, use_stdcall=use_stdcall)

    def save(self, path: os.PathLike | str) -> None:
        cfg = configparser.ConfigParser()
        cfg[self.EOM1_SECTION] = {"board_idx": str(self.board_idx),
                                  **{k: repr(v) for k, v in zip(_EOM_PARAM_KEYS, self.eom1)}}
        cfg[self.EOM2_SECTION] = {k: repr(v) for k, v in zip(_EOM_PARAM_KEYS, self.eom2)}
        cfg[self.DLL_SECTION] = {
            "dll_path": self.dll_path or "",
            "use_stdcall": "yes" if self.use_stdcall else "no",
        }
        with Path(path).open("w", encoding="utf-8") as f:
            cfg.write(f)


class DualEOMController:
    """ctypes wrapper for the EOM2_* dual-controller API of the DLL."""

    def __init__(self, dll_path: os.PathLike | str, config: DualEOMConfig, use_stdcall: bool = False):
        self._config = config
        self._dll_path = str(dll_path)
        self._lib = EOMController._load_library(self._dll_path, use_stdcall or config.use_stdcall)
        self._bind_signatures()
        self._handle: Optional[int] = None

        self._handle = self._lib.EOM2_Create(
            c_short(self._config.board_idx),
            *(c_double(v) for v in self._config.eom1),
            *(c_double(v) for v in self._config.eom2),
        )
        if not self._handle:
            raise RuntimeError("EOM2_Create returned NULL (failed to create DualEOMController instance).")

    @classmethod
    def from_config_path(cls, config_path: os.PathLike | str) -> "DualEOMController":
        cfg = DualEOMConfig.from_config_file(config_path)
        return cls(cfg.dll_path, cfg, use_stdcall=cfg.use_stdcall)

    def _bind_signatures(self) -> None:
        # void* EOM2_Create(short, double x12)
        self._lib.EOM2_Create.argtypes = [c_short] + [c_double] * 12
        self._lib.EOM2_Create.restype = c_void_p

        # bool EOM2_Start(void*)
        self._lib.EOM2_Start.argtypes = [c_void_p]
        self._lib.EOM2_Start.restype = c_bool

        # void EOM2_Stop(void*)
        self._lib.EOM2_Stop.argtypes = [c_void_p]
        self._lib.EOM2_Stop.restype = None

        # void EOM2_GetValues(void*, (double*, double*, bool*) x2)
        self._lib.EOM2_GetValues.argtypes = [c_void_p] + [POINTER(c_double), POINTER(c_double), POINTER(c_bool)] * 2
        self._lib.EOM2_GetValues.restype = None

        # void EOM2_Destroy(void*)
        self._lib.EOM2_Destroy.argtypes = [c_void_p]
        self._lib.EOM2_Destroy.restype = None

    def start(self) -> bool:
        self._require_handle()
        return bool(self._lib.EOM2_Start(self._handle))

    def stop(self) -> None:
        if self._handle:
            self._lib.EOM2_Stop(self._handle)

    def get_values(self) -> Tuple[Tuple[float, float, bool], Tuple[float, float, bool]]:
        """Return ((diode1, bias1, laser1_on), (diode2, bias2, laser2_on))."""
        self._require_handle()
        d1, b1, l1 = c_double(0.0), c_double(0.0), c_bool(False)
        d2, b2, l2 = c_double(0.0), c_double(0.0), c_bool(False)
        self._lib.EOM2_GetValues(
            self._handle,
            ctypes.byref(d1), ctypes.byref(b1), ctypes.byref(l1),
            ctypes.byref(d2), ctypes.byref(b2), ctypes.byref(l2),
        )
        return ((float(d1.value), float(b1.value), bool(l1.value)),
                (float(d2.value), float(b2.value), bool(l2.value)))

    def close(self) -> None:
        if self._handle:
            try:
                self._lib.EOM2_Stop(self._handle)
            except Exception:
                pass
            try:
                self._lib.EOM2_Destroy(self._handle)
            finally:
                self._handle = None

    def __enter__(self) -> "DualEOMController":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def _require_handle(self) -> None:
        if not self._handle:
            raise RuntimeError("Dual EOM controller has been closed or not initialized.")
