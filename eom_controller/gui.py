"""PyQt6 widgets for the dual EOM controller.
EomWidget is a seed-power indicator for one EomChannel: green means seed power
is acceptable, red means too low; it shows photodiode/bias readings and can
restart the shared control loop. DualEomWindow shows both channels; run()
starts it as a standalone application."""

from __future__ import annotations

from typing import Optional, Sequence

from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtWidgets import (
    QApplication, QGroupBox, QLabel, QPushButton, QVBoxLayout, QWidget,
)

from .channels import EomChannel, SharedEom

_SEED_OK_STYLE = "background-color:#2e7d32;color:white;font-weight:bold;"
_SEED_BAD_STYLE = "background-color:#b71c1c;color:white;font-weight:bold;"
_ERR_STYLE = "color:#fca5a5;font-weight:bold;"


class EomWidget(QGroupBox):
    """Display seed-power status for a single EOM channel."""

    def __init__(self, channel: Optional[object],
                 title: str = "Seed indicator", poll_ms: int = 250) -> None:
        super().__init__(title)
        self.channel = channel

        layout = QVBoxLayout(self)

        # Large status indicator for seed power.
        self.indicator = QLabel("Seed-Laser: --")
        self.indicator.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.indicator.setMinimumHeight(44)
        layout.addWidget(self.indicator)

        # Channel measurements.
        self.value_label = QLabel("PD --   B --")
        self.value_label.setTextFormat(Qt.TextFormat.PlainText)
        self.value_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(self.value_label)

        # Button to restart the shared control loop.
        self.restart_btn = QPushButton("Restart control loop")
        layout.addWidget(self.restart_btn)

        if self.channel is None:
            self._set_disconnected()
            return

        self.restart_btn.clicked.connect(self._restart)
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._poll)
        self._timer.start(poll_ms)

        # Start the background control loop if not already active.
        try:
            if not self.channel.is_running:
                self.channel.start()
        except Exception as exc:                      # pragma: no cover
            self._set_error(exc)
            return
        self._poll()

    # ------------------------------------------------------------------ #
    def _set_seed(self, good: bool) -> None:
        if good:
            self.indicator.setText("● Seed laser present (power OK)")
            self.indicator.setStyleSheet(_SEED_OK_STYLE)
        else:
            self.indicator.setText("○ No seed laser (power too low)")
            self.indicator.setStyleSheet(_SEED_BAD_STYLE)

    def _set_disconnected(self) -> None:
        self.restart_btn.setEnabled(False)
        self.indicator.setText("⚠ EOM disconnected")
        self.indicator.setStyleSheet(_ERR_STYLE)
        self.value_label.setText("PD --   B --")
        self.setStyleSheet("QGroupBox { border:1px solid #b71c1c; }")

    def _set_error(self, exc: Exception) -> None:
        self.restart_btn.setEnabled(False)
        self.indicator.setText(f"⚠ EOM error: {exc}")
        self.indicator.setStyleSheet(_ERR_STYLE)

    def _restart(self) -> None:
        try:
            self.channel.stop()
            self.channel.start()
        except Exception as exc:                      # pragma: no cover
            self._set_error(exc)
            return
        self._poll()

    def _poll(self) -> None:
        try:
            diode, bias, good = self.channel.value()
        except Exception as exc:                      # pragma: no cover
            self._set_error(exc)
            self._timer.stop()
            return
        self._set_seed(bool(good))
        self.value_label.setText(f"PD {diode:.3f}   B {bias:.3f}")


class DualEomWindow(QWidget):
    """Both channels of one shared dual EOM controller (EOM 1 = AO0/AI0, EOM 2 = AO1/AI1).
    Closing the window stops the shared control loop and releases the controller."""

    def __init__(self, shared: SharedEom, title: str = "Dual EOM controller",
                 poll_ms: int = 250) -> None:
        super().__init__()
        self.setWindowTitle(title)
        self.shared = shared
        layout = QVBoxLayout(self)
        self.channel_widgets: Sequence[EomWidget] = [
            EomWidget(EomChannel(shared, index),
                      title=f"EOM {index + 1} (AO{index} / AI{index})",
                      poll_ms=poll_ms)
            for index in (0, 1)
        ]
        for widget in self.channel_widgets:
            layout.addWidget(widget)

    def closeEvent(self, event) -> None:
        for widget in self.channel_widgets:
            timer = getattr(widget, "_timer", None)
            if timer is not None:
                timer.stop()
        try:
            self.shared.close()
        except Exception:
            pass
        event.accept()


def run(controller, title: str = "Dual EOM controller") -> int:
    """Show a DualEomWindow for a DualEOMController/MockDualEOMController (or a
    SharedEom) and run the Qt event loop."""
    shared = controller if isinstance(controller, SharedEom) else SharedEom(controller)
    app = QApplication.instance() or QApplication([])
    window = DualEomWindow(shared, title=title)
    window.show()
    return app.exec()
