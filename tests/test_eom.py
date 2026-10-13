"""Hardware-free tests: configuration, DLL path resolution, channels, mock, widgets."""

import os
from pathlib import Path

import pytest

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

import eom_controller
from eom_controller import (
    BUNDLED_DLL, DualEOMConfig, EomChannel, MockDualEOMController, SharedEom,
    resolve_dll_path,
)

DEFAULT_INI = Path(eom_controller.__file__).parent / "default_eom.ini"


def test_bundled_dll_is_shipped():
    assert BUNDLED_DLL.is_file()


def test_resolve_dll_path(tmp_path):
    assert resolve_dll_path("") == BUNDLED_DLL
    assert resolve_dll_path(None) == BUNDLED_DLL
    assert resolve_dll_path("x/a.dll", base_dir=tmp_path) == tmp_path / "x" / "a.dll"
    absolute = tmp_path / "b.dll"
    assert resolve_dll_path(absolute, base_dir="ignored") == absolute


def test_default_ini_uses_bundled_dll():
    cfg = DualEOMConfig.from_config_file(DEFAULT_INI)
    assert cfg.board_idx == 1
    assert Path(cfg.dll_path) == BUNDLED_DLL
    assert cfg.eom1 == (-6.0, 7.0, 0.0, 1.0, 0.18, 0.5)
    assert len(cfg.eom2) == 6


def test_relative_dll_path_resolves_against_ini(tmp_path):
    ini = tmp_path / "eom.ini"
    ini.write_text(DEFAULT_INI.read_text().replace("dll_path =", "dll_path = sub/my.dll"))
    cfg = DualEOMConfig.from_config_file(ini)
    assert Path(cfg.dll_path) == tmp_path / "sub" / "my.dll"


def test_config_roundtrip(tmp_path):
    cfg = DualEOMConfig.from_config_file(DEFAULT_INI)
    cfg.save(tmp_path / "copy.ini")
    again = DualEOMConfig.from_config_file(tmp_path / "copy.ini")
    assert (again.board_idx, again.eom1, again.eom2) == (cfg.board_idx, cfg.eom1, cfg.eom2)


def test_shared_channels_with_mock():
    shared = SharedEom(MockDualEOMController())
    ch0, ch1 = EomChannel(shared, 0), EomChannel(shared, 1)
    assert ch0.value() == (0.0, 0.0, False)          # not running
    ch0.start()
    assert ch0.is_running and ch1.is_running         # start affects both
    assert ch0.value()[2] is True                    # channel 0 seed always present
    ch1.stop()
    assert not ch0.is_running
    with pytest.raises(ValueError):
        EomChannel(shared, 2)
    shared.close()
    with pytest.raises(RuntimeError):
        shared.values()


def test_widgets_offscreen():
    pytest.importorskip("PyQt6")
    from PyQt6.QtWidgets import QApplication
    from eom_controller.gui import DualEomWindow, EomWidget

    app = QApplication.instance() or QApplication([])
    shared = SharedEom(MockDualEOMController())
    window = DualEomWindow(shared)
    assert shared.running                            # widgets auto-start the loop
    assert "Seed laser present" in window.channel_widgets[0].indicator.text()
    window.close()
    assert not shared.running
    assert "disconnected" in EomWidget(None).indicator.text()
    app.processEvents()
