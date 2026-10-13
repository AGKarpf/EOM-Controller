"""In-memory dual EOM controller for hardware-free GUI tests.
Implements start, stop, get_values and close without an NI board or DLL."""

from __future__ import annotations

import math
from typing import Tuple


class MockDualEOMController:
    """Simulate two EOM channels on a single board."""

    def __init__(self, board_idx: int = 1) -> None:
        self.board_idx = board_idx
        self._running = False
        self._tick = 0
        self._closed = False

    def start(self) -> bool:
        if self._closed:
            raise RuntimeError("Controller was closed.")
        self._running = True
        return True

    def stop(self) -> None:
        self._running = False

    def get_values(self) -> Tuple[Tuple[float, float, bool],
                                  Tuple[float, float, bool]]:
        """((diode1, bias1, laser1_on), (diode2, bias2, laser2_on))."""
        if self._closed:
            raise RuntimeError("Controller was closed.")
        self._tick += 1
        if not self._running:
            return ((0.0, 0.0, False), (0.0, 0.0, False))
        t = self._tick * 0.2
        d1 = 0.30 + 0.03 * math.sin(t)
        d2 = 0.28 + 0.03 * math.sin(t + 1.1)
        b1 = 2.50 + 0.10 * math.sin(t * 0.5)
        b2 = 2.40 + 0.10 * math.sin(t * 0.5 + 0.7)
        # Seed power flag per channel:
        #   Channel 0 -> seed always present.
        #   Channel 1 -> periodic dropouts demonstrate the red indicator.
        good1 = True
        good2 = math.sin(t * 0.4) > -0.2
        return ((round(d1, 3), round(b1, 3), good1),
                (round(d2, 3), round(b2, 3), bool(good2)))

    def close(self) -> None:
        self._running = False
        self._closed = True

    def __enter__(self) -> "MockDualEOMController":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()
