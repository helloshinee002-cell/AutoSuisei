#!/usr/bin/env python3
"""
Compare an extraction CSV against the user-verified ground_truth.csv.

Both files key on filename. Reports per-row accuracy on the pc_no column.

Usage:
    python compare_to_ground_truth.py <extraction.csv> <ground_truth.csv>
"""
import csv
import sys


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: compare_to_ground_truth.py <extraction.csv> <ground_truth.csv>",
              file=sys.stderr)
        return 1

    ext_path = sys.argv[1]
    gt_path = sys.argv[2]

    gt: dict[str, str] = {}
    with open(gt_path, encoding="utf-8") as f:
        for row in csv.DictReader(f):
            gt[row["filename"]] = row["pc_no"]

    correct = 0
    wrong = 0
    missed = 0
    extra = 0
    total = 0
    wrong_examples: list[tuple[str, str, str]] = []
    missed_examples: list[tuple[str, str]] = []

    with open(ext_path, encoding="utf-8") as f:
        for row in csv.DictReader(f):
            fn = row["filename"]
            ext_pc = row.get("pc_no", "")
            if fn not in gt:
                continue
            total += 1
            truth = gt[fn]
            if truth == ext_pc:
                correct += 1
            elif not ext_pc and truth:
                missed += 1
                if len(missed_examples) < 10:
                    missed_examples.append((fn, truth))
            elif ext_pc and not truth:
                extra += 1
            else:
                wrong += 1
                if len(wrong_examples) < 10:
                    wrong_examples.append((fn, ext_pc, truth))

    pct = 100.0 * correct / total if total else 0.0
    print(f"Total rows compared: {total}")
    print(f"Correct: {correct} ({pct:.1f}%)")
    print(f"Wrong:   {wrong}")
    print(f"Missed:  {missed} (extractor empty, truth has value)")
    print(f"Extra:   {extra} (extractor has value, truth empty)")
    if wrong_examples:
        print("\nWrong examples (extractor vs truth):")
        for fn, e, t in wrong_examples:
            print(f"  {fn}: ext={e} truth={t}")
    if missed_examples:
        print("\nMissed examples (truth value):")
        for fn, t in missed_examples:
            print(f"  {fn}: truth={t}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
