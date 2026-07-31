#!/usr/bin/env python3
"""Design and verify the 62.5 MSPS three-stage decimating FIR chain.

The generated coefficients are normalized to unity DC gain before Q1.17
quantization.  The script evaluates the complete multirate alias map, not just
the three individual low-pass responses.
"""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

import numpy as np
import scipy
from scipy import signal


Q_BITS = 17
WEIGHTS = (1.0, 0.2)
STAGES = (
    {"name": "stage1", "fs_hz": 62_500_000.0, "pass_hz": 500_000.0,
     "stop_hz": 14_625_000.0, "taps": 15, "decimation": 4},
    {"name": "stage2", "fs_hz": 15_625_000.0, "pass_hz": 500_000.0,
     "stop_hz": 2_906_250.0, "taps": 25, "decimation": 4},
    {"name": "stage3", "fs_hz": 3_906_250.0, "pass_hz": 500_000.0,
     "stop_hz": 1_000_000.0, "taps": 39, "decimation": 2},
)


def design_stage(spec: dict) -> tuple[np.ndarray, np.ndarray]:
    coeff = signal.remez(
        spec["taps"],
        [0.0, spec["pass_hz"], spec["stop_hz"], spec["fs_hz"] / 2.0],
        [1.0, 0.0],
        weight=WEIGHTS,
        fs=spec["fs_hz"],
        maxiter=500,
        grid_density=64,
    )
    coeff /= np.sum(coeff)
    quantized = np.rint(coeff * (1 << Q_BITS)).astype(np.int64)
    return coeff, quantized


def alias_frequency(frequency_hz: float, sample_rate_hz: float) -> float:
    folded = frequency_hz % sample_rate_hz
    return min(folded, sample_rate_hz - folded)


def response_at(coeff: np.ndarray, sample_rate_hz: float,
                frequency_hz: float) -> float:
    index = np.arange(coeff.size, dtype=np.float64)
    angle = -2j * np.pi * frequency_hz * index / sample_rate_hz
    return float(abs(np.sum(coeff * np.exp(angle))))


def cascade_response(designs: list[dict], frequency_hz: float) -> float:
    amplitude = 1.0
    current_frequency = frequency_hz
    for item in designs:
        current_frequency = alias_frequency(current_frequency,
                                            item["fs_hz"])
        amplitude *= response_at(item["coeff_q"], item["fs_hz"],
                                 current_frequency)
        current_frequency = alias_frequency(
            current_frequency, item["fs_hz"] / item["decimation"]
        )
    return amplitude


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    designs: list[dict] = []
    for spec in STAGES:
        coeff_float, coeff_int = design_stage(spec)
        item = dict(spec)
        item["coeff_int"] = coeff_int
        item["coeff_q"] = coeff_int.astype(np.float64) / (1 << Q_BITS)
        item["float_sum"] = float(np.sum(coeff_float))
        designs.append(item)

    frequencies = np.linspace(0.0, 31_250_000.0, 250_001)
    amplitudes = np.array([cascade_response(designs, value)
                           for value in frequencies])
    passband = amplitudes[frequencies <= 500_000.0]
    stopband = amplitudes[frequencies >= 1_000_000.0]
    ripple_db = float(20.0 * np.log10(np.max(passband) / np.min(passband)))
    stop_db = float(-20.0 * np.log10(np.max(stopband)))
    worst_stop_frequency = float(
        frequencies[frequencies >= 1_000_000.0][np.argmax(stopband)]
    )

    summary = {
        "scipy_version": scipy.__version__,
        "coefficient_format": "signed Q1.17",
        "weights": list(WEIGHTS),
        "raw_sample_rate_hz": 62_500_000,
        "total_decimation": 32,
        "analysis_sample_rate_hz": 1_953_125.0,
        "fft_size": 4096,
        "fft_bin_hz": 1_953_125.0 / 4096.0,
        "cascade_passband_ripple_db": ripple_db,
        "cascade_minimum_stopband_db": stop_db,
        "worst_stopband_frequency_hz": worst_stop_frequency,
        "stages": [],
    }
    for item in designs:
        summary["stages"].append({
            "name": item["name"],
            "fs_hz": item["fs_hz"],
            "pass_hz": item["pass_hz"],
            "stop_hz": item["stop_hz"],
            "taps": item["taps"],
            "decimation": item["decimation"],
            "integer_sum": int(np.sum(item["coeff_int"])),
            "unique_coefficients": [
                int(value) for value in item["coeff_int"][:item["taps"] // 2 + 1]
            ],
        })

    print(json.dumps(summary, indent=2))
    if args.output_dir is not None:
        args.output_dir.mkdir(parents=True, exist_ok=True)
        (args.output_dir / "fir62m5_summary.json").write_text(
            json.dumps(summary, indent=2) + "\n", encoding="utf-8"
        )
        with (args.output_dir / "fir62m5_response.csv").open(
            "w", newline="", encoding="utf-8"
        ) as stream:
            writer = csv.writer(stream)
            writer.writerow(("frequency_hz", "amplitude", "amplitude_db"))
            for frequency, amplitude in zip(frequencies[::25], amplitudes[::25]):
                writer.writerow((f"{frequency:.3f}", f"{amplitude:.12g}",
                                 f"{20.0 * np.log10(max(amplitude, 1e-20)):.9f}"))

    if args.check and (ripple_db > 0.01 or stop_db < 60.0):
        raise SystemExit(
            f"response failed: ripple={ripple_db:.6f} dB, stop={stop_db:.6f} dB"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
