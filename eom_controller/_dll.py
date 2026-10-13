"""Locate and preload EOM_Controller_NI.dll.
The DLL (and the NI-DAQmx runtime it links) must be loaded BEFORE Qt: Qt ships
its own Visual C++ runtime, and if that loads first the DLL binds to it and
crashes inside EOM_Create with an access violation."""

from __future__ import annotations

import ctypes
import os
import sys
from pathlib import Path
from typing import Optional

#: DLL shipped inside this package (built from cpp/ with CMake).
BUNDLED_DLL = Path(__file__).resolve().parent / "bin" / "EOM_Controller_NI.dll"


def resolve_dll_path(dll_path: Optional[os.PathLike | str] = None,
                     base_dir: Optional[os.PathLike | str] = None) -> Path:
    """Return the DLL to load: the bundled one if dll_path is empty, otherwise
    dll_path (relative paths resolved against base_dir, default: cwd)."""
    raw = str(dll_path or "").strip()
    if not raw:
        return BUNDLED_DLL
    path = Path(raw)
    if not path.is_absolute() and base_dir is not None:
        path = Path(base_dir) / path
    return path


def preload_dll(dll_path: Optional[os.PathLike | str] = None) -> bool:
    """Best-effort load of the DLL so the correct C++/NI runtime is resident
    before Qt is imported. Return True on success; never raise."""
    if sys.platform != "win32":
        return False
    try:
        ctypes.CDLL(str(resolve_dll_path(dll_path)))
        return True
    except OSError:
        # Not fatal: the controller loads the DLL again later and reports
        # real errors (missing file, missing NI-DAQmx runtime) properly.
        return False
