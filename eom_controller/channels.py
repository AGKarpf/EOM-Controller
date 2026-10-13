"""One shared dual EOM controller, with a view for each laser.
Channel 0 maps to EOM 1 (AO0/AI0); channel 1 maps to EOM 2 (AO1/AI1).
Start and stop affect both channels because the DLL uses one worker thread."""

from __future__ import annotations

from typing import Tuple


class SharedEom:
    """Wrap one hardware or mock dual EOM controller and its shared running state."""

    def __init__(self, controller) -> None:
        self.controller = controller
        self.running = False

    def start(self) -> bool:
        ok = bool(self.controller.start())
        self.running = True
        return ok

    def stop(self) -> None:
        self.controller.stop()
        self.running = False

    def values(self):
        """((diode1, bias1, laser1), (diode2, bias2, laser2))."""
        return self.controller.get_values()

    def close(self) -> None:
        try:
            self.controller.close()
        finally:
            self.running = False


class EomChannel:
    """One laser's view of a shared EOM channel.
    Start/stop affect both channels; measurements return only this channel."""

    def __init__(self, shared: SharedEom, index: int) -> None:
        if index not in (0, 1):
            raise ValueError("EOM channel index must be 0 or 1.")
        self.shared = shared
        self.index = index

    @property
    def is_running(self) -> bool:
        return self.shared.running

    def start(self) -> bool:
        return self.shared.start()

    def stop(self) -> None:
        self.shared.stop()

    def value(self) -> Tuple[float, float, bool]:
        """Return (diode, bias, laser_on) for the assigned channel."""
        return self.shared.values()[self.index]
