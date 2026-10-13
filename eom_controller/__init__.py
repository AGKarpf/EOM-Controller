"""EOM controller for NI-DAQmx based EOM bias regulation.
Wraps EOM_Controller_NI.dll (bundled, sources in cpp/) via ctypes, maps the
dual controller's two channels to per-laser views and offers a hardware-free
mock. PyQt6 widgets live in eom_controller.gui and are imported explicitly so
the core works without Qt. Import this package before Qt (see preload_dll)."""

from ._dll import BUNDLED_DLL, preload_dll, resolve_dll_path
from .regulator import (
    DualEOMConfig, DualEOMController, EOMConfig, EOMController,
)
from .channels import EomChannel, SharedEom
from .mock import MockDualEOMController

__all__ = [
    "BUNDLED_DLL", "preload_dll", "resolve_dll_path",
    "EOMConfig", "EOMController", "DualEOMConfig", "DualEOMController",
    "SharedEom", "EomChannel",
    "MockDualEOMController",
]
__version__ = "2.0.0"
