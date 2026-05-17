#!/usr/bin/env python3
"""
Sanity-check extracted PC No. against the range hint in the filename.

Train2 folder is named "Laptop 301-400" so extracted PC No. should land
in [301, 400]. Anything outside likely came from OCR'd model numbers,
years, percentages, etc.

Usage:
    python validate_pc_range.py <csv> <lo> <hi>
"""
import csv
import sys
from collections import Counter


def main() -> int:
    if len(sys.argv) < 4:
        print("usage: validate_pc_range.py <csv> <lo> <hi>", file=sys.stderr)
        return 1

    csv_path = sys.argv[1]
    lo, hi = int(sys.argv[2]), int(sys.argv[3])

    in_range = 0
    out_range_vals: Counter[str] = Counter()
    empty = 0
    total = 0

    with open(csv_path, encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            total += 1
            pc = row["pc_no"]
            if not pc:
                empty += 1
                continue
            try:
                n = int(pc)
            except ValueError:
                out_range_vals[pc] += 1
                continue
            if lo <= n <= hi:
                in_range += 1
            else:
                out_range_vals[pc] += 1

    print(f"Total: {total}")
    print(f"PC No. in [{lo},{hi}]: {in_range}/{total} ({100*in_range/total:.1f}%)")
    print(f"Empty: {empty}/{total}")
    print(f"Out of range / non-numeric: {sum(out_range_vals.values())}/{total}")
    if out_range_vals:
        print("\nTop 20 out-of-range values:")
        for val, count in out_range_vals.most_common(20):
            print(f"  {val:>6}: {count}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
