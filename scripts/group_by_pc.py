#!/usr/bin/env python3
"""
Group extracted-asset CSV rows by pc_no.

Input columns: photo_index,filename,pc_no,serial_no,batch_id,photo_date,pc_range,
               mean_confidence,line_count,warnings

Output columns: pc_no,photo_count,serials,filenames,best_confidence

Usage:
    python group_by_pc.py <in.csv> <out.csv>
"""
import csv
import sys
from collections import defaultdict


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: group_by_pc.py <in.csv> <out.csv>", file=sys.stderr)
        return 1

    in_path = sys.argv[1]
    out_path = sys.argv[2]

    by_pc: dict[str, dict] = defaultdict(lambda: {
        "photo_count": 0,
        "serials": set(),
        "filenames": [],
        "best_confidence": 0.0,
    })
    missing = []

    with open(in_path, encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            pc = row["pc_no"]
            if not pc:
                missing.append(row["filename"])
                continue
            entry = by_pc[pc]
            entry["photo_count"] += 1
            if row["serial_no"]:
                entry["serials"].add(row["serial_no"])
            entry["filenames"].append(row["filename"])
            try:
                conf = float(row["mean_confidence"])
                if conf > entry["best_confidence"]:
                    entry["best_confidence"] = conf
            except ValueError:
                pass

    with open(out_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["pc_no", "photo_count", "serials",
                         "filenames", "best_confidence"])
        # Sort numerically when possible
        def key(k: str):
            try:
                return (0, int(k))
            except ValueError:
                return (1, k)
        for pc in sorted(by_pc.keys(), key=key):
            e = by_pc[pc]
            writer.writerow([
                pc,
                e["photo_count"],
                ";".join(sorted(e["serials"])),
                ";".join(e["filenames"]),
                f"{e['best_confidence']:.3f}",
            ])

    print(f"Unique PCs: {len(by_pc)}")
    print(f"PCs with >=1 serial: {sum(1 for e in by_pc.values() if e['serials'])}")
    print(f"Photos missing PC No.: {len(missing)}")
    print(f"Output: {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
