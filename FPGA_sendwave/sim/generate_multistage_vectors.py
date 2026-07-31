"""Generate deterministic RTL/golden-model vectors for the 62.5 MSPS FIR."""

from pathlib import Path
import random

STAGES = (
    ([-544, -1689, -2368, -154, 6975, 18109, 28679, 33053], 4),
    ([169, 412, 534, 144, -1003, -2590, -3525, -2251, 2399,
      10250, 19395, 26788, 29629], 4),
    ([-27, -16, 83, 131, -92, -408, -177, 688, 977, -430,
      -2166, -1143, 2788, 4436, -937, -8849, -6625, 12742,
      38945, 51233], 2),
)


def symmetric(unique):
    return unique[:-1] + [unique[-1]] + unique[-2::-1]


def round_shift(value, bits):
    return (value + ((1 << (bits - 1)) if value >= 0
                     else ((1 << (bits - 1)) - 1))) >> bits


class Stage:
    def __init__(self, unique, decimation):
        self.coefficients = symmetric(unique)
        self.decimation = decimation
        self.history = []
        self.count = 0

    def push(self, value):
        self.history.insert(0, value)
        self.history = self.history[:len(self.coefficients)]
        self.count += 1
        if self.count % self.decimation or len(self.history) < len(self.coefficients):
            return None
        result = round_shift(sum(x * c for x, c in zip(
            self.history, self.coefficients)), 17)
        return max(-65536, min(65535, result))


def main():
    random.seed(0x625032)
    raw = [random.randint(-512, 511) for _ in range(20000)]
    stages = [Stage(*description) for description in STAGES]
    expected = []
    for sample in raw:
        value = sample << 4
        for stage in stages:
            value = stage.push(value)
            if value is None:
                break
        if value is not None:
            output = round_shift(value, 4)
            expected.append(max(-2048, min(2047, output)))

    output_dir = Path(__file__).resolve().parent / "vectors"
    output_dir.mkdir(exist_ok=True)
    (output_dir / "raw_q12.mem").write_text(
        "".join(f"{value & 0xfff:03x}\n" for value in raw), encoding="ascii")
    (output_dir / "expected_q12.mem").write_text(
        "".join(f"{value & 0xfff:03x}\n" for value in expected), encoding="ascii")
    (output_dir / "vector_counts.vh").write_text(
        f"`define RAW_VECTOR_COUNT {len(raw)}\n"
        f"`define EXPECTED_VECTOR_COUNT {len(expected)}\n", encoding="ascii")
    print(f"raw={len(raw)} expected={len(expected)}")


if __name__ == "__main__":
    main()
