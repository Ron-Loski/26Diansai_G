"""Verify the weak-harmonic FFT gate against output-code quantization."""

import numpy as np


SAMPLE_RATE_HZ = 62_500_000.0 / 32.0
SAMPLE_COUNT = 4096
MV_PER_CODE = (10_000.0 / 4096.0) * 0.968936
NOMINAL_PEAK_MV = 2.5
GATE_MARGIN = 0.8


def interpolated_peak(magnitude: np.ndarray, frequency_hz: float) -> float:
    bin_hz = SAMPLE_RATE_HZ / SAMPLE_COUNT
    center = int(round(frequency_hz / bin_hz))
    y0, y1, y2 = magnitude[center - 1:center + 2]
    denominator = y0 - 2.0 * y1 + y2
    delta = (
        0.5 * (y0 - y2) / denominator
        if abs(denominator) > 1.0e-20
        else 0.0
    )
    delta = np.clip(delta, -0.5, 0.5)
    return 4.0 * (y1 - 0.25 * (y0 - y2) * delta) / SAMPLE_COUNT


def main() -> None:
    rng = np.random.default_rng(20260731)
    sample_index = np.arange(SAMPLE_COUNT)
    window = 0.5 - 0.5 * np.cos(
        2.0 * np.pi * sample_index / (SAMPLE_COUNT - 1)
    )
    fundamental_peak_code = 25.0 / MV_PER_CODE
    harmonic_peak_code = NOMINAL_PEAK_MV / MV_PER_CODE
    old_gate_code = harmonic_peak_code
    margin_gate_code = harmonic_peak_code * GATE_MARGIN
    estimates = []

    for _ in range(2000):
        fundamental_phase, harmonic_phase = rng.uniform(
            -np.pi, np.pi, size=2
        )
        samples = (
            fundamental_peak_code
            * np.cos(
                2.0 * np.pi * 10_000.0 * sample_index /
                SAMPLE_RATE_HZ + fundamental_phase
            )
            + harmonic_peak_code
            * np.cos(
                2.0 * np.pi * 20_000.0 * sample_index /
                SAMPLE_RATE_HZ + harmonic_phase
            )
        )
        samples = np.rint(samples)
        samples -= np.mean(samples)
        magnitude = np.abs(np.fft.fft(samples * window))[:SAMPLE_COUNT // 2]
        estimates.append(interpolated_peak(magnitude, 20_000.0))

    estimates = np.asarray(estimates)
    old_pass_rate = np.mean(estimates >= old_gate_code)
    margin_pass_rate = np.mean(estimates >= margin_gate_code)

    print(f"estimated_peak_min_mv={estimates.min() * MV_PER_CODE:.6f}")
    print(f"estimated_peak_max_mv={estimates.max() * MV_PER_CODE:.6f}")
    print(f"old_gate_pass_rate={old_pass_rate:.6f}")
    print(f"margin_gate_pass_rate={margin_pass_rate:.6f}")

    if margin_pass_rate < 1.0:
        raise SystemExit("FAIL: 20% margin did not cover all phase cases")
    print("PASS weak-harmonic acquisition margin")


if __name__ == "__main__":
    main()
