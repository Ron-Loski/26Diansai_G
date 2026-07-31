"""Verify the exact fixed-point FIR coefficients used by the FPGA."""

from pathlib import Path
import re

import numpy as np


FS_HZ = 25_000_000.0
Q_BITS = 17
TAP_COUNT = 159


def load_unique_coefficients() -> np.ndarray:
    rtl_path = (
        Path(__file__).resolve().parents[1]
        / "rtl"
        / "Ad9233DecimatingFir.V"
    )
    text = rtl_path.read_text(encoding="utf-8")
    matches = re.findall(
        r"7'd(\d+): coefficient = (-?)18'sd(\d+);",
        text,
    )
    values = {int(i): (-1 if sign else 1) * int(v)
              for i, sign, v in matches}
    if sorted(values) != list(range(80)):
        raise RuntimeError("Expected coefficient indices 0 through 79")
    return np.array([values[i] for i in range(80)], dtype=np.int64)


def main() -> None:
    unique = load_unique_coefficients()
    coefficients = np.concatenate((unique[:79], unique[79:], unique[78::-1]))
    if len(coefficients) != TAP_COUNT:
        raise RuntimeError("Unexpected FIR length")
    if int(coefficients.sum()) != (1 << Q_BITS):
        raise RuntimeError("Fixed-point coefficients do not have unity DC gain")

    fft_size = 1 << 20
    response = np.fft.rfft(coefficients / (1 << Q_BITS), fft_size)
    frequency = np.fft.rfftfreq(fft_size, 1.0 / FS_HZ)
    magnitude = np.abs(response)

    passband = magnitude[frequency <= 500_000.0]
    stopband = magnitude[frequency >= 1_000_000.0]
    ripple_db = 20.0 * np.log10(passband.max() / passband.min())
    stopband_db = -20.0 * np.log10(stopband.max())

    print(f"coefficient_sum={coefficients.sum()}")
    print(f"passband_ripple_db={ripple_db:.6f}")
    print(f"minimum_stopband_db={stopband_db:.6f}")
    for test_hz in (
        10_000.0,
        100_000.0,
        250_000.0,
        500_000.0,
        1_000_000.0,
        1_100_000.0,
        1_500_000.0,
        2_000_000.0,
        4_000_000.0,
        5_000_000.0,
        10_000_000.0,
    ):
        index = int(round(test_hz / FS_HZ * fft_size))
        gain_db = 20.0 * np.log10(max(magnitude[index], 1e-20))
        print(f"gain_db_at_{test_hz:.0f}_hz={gain_db:.6f}")

    if ripple_db > 0.05:
        raise SystemExit("FAIL: passband ripple exceeds 0.05 dB")
    if stopband_db < 50.0:
        raise SystemExit("FAIL: stopband rejection is below 50 dB")
    print("PASS fixed-point FIR response")


if __name__ == "__main__":
    main()
