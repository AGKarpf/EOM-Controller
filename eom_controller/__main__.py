"""Standalone dual EOM GUI: py -m eom_controller [--ini eom.ini] [--mock].
Without --ini, eom.ini in the current folder is used, else the packaged default.
The DLL is preloaded before Qt is imported (see eom_controller._dll). Startup
errors are written to eom_controller.log and shown in a message box."""

from __future__ import annotations

import argparse
import datetime
import sys
import traceback
from pathlib import Path

from . import DualEOMConfig, DualEOMController, MockDualEOMController, preload_dll

DEFAULT_INI = Path(__file__).resolve().parent / "default_eom.ini"
LOGFILE = Path("eom_controller.log")


def _log(text: str) -> None:
    stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    try:
        with LOGFILE.open("a", encoding="utf-8") as stream:
            stream.write(f"[{stamp}] {text}\n")
    except OSError:
        pass


def _error_box(title: str, text: str) -> None:
    try:
        import ctypes
        ctypes.windll.user32.MessageBoxW(0, text, title, 0x10)  # MB_ICONERROR
    except Exception:
        print(f"{title}: {text}", file=sys.stderr)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog="eom_controller", description=__doc__)
    parser.add_argument("--ini", type=Path, help="INI file with [EOM1]/[EOM2]/[EOM_DLL]")
    parser.add_argument("--mock", action="store_true", help="simulate the controller (no NI board/DLL)")
    args = parser.parse_args(argv)
    try:
        if args.mock:
            controller = MockDualEOMController()
        else:
            ini = args.ini or (Path("eom.ini") if Path("eom.ini").exists() else DEFAULT_INI)
            config = DualEOMConfig.from_config_file(ini)
            preload_dll(config.dll_path)          # before any Qt import
            controller = DualEOMController(config.dll_path, config,
                                           use_stdcall=config.use_stdcall)
        from .gui import run
        return run(controller, title="Dual EOM controller (simulated)" if args.mock
                   else "Dual EOM controller")
    except Exception as exc:
        _log("".join(traceback.format_exception(type(exc), exc, exc.__traceback__)))
        _error_box("EOM controller failed to start", f"{exc}\n\nSee {LOGFILE.resolve()}.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
