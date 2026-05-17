#!/usr/bin/env python3
"""
Dedupe Dell service-tag readings per PC.

When the same PC was photographed multiple times, OCR may return slightly
different serial strings — usually a single-character confusion like
"C7WS9M2" vs "C7WS9N2" (M↔N) or "614ZFL2" vs "6L4ZFL2" (1↔L). Plain
group-by-PC then concatenates all variants, which is noisy.

This script clusters readings within edit distance ≤1 across photos of
the same PC, totals their OCR confidences, and picks the cluster with
the highest total confidence. Within that winning cluster, the single
reading with the highest individual confidence becomes canonical.

Input:  per-photo CSV from bulk_extract.py (filename,pc_no,serial_no,...
        ,mean_confidence,...)
Output: per-PC CSV with one canonical serial per pc_no, plus rejected
        candidates for audit.

Usage:
    python dedupe_serials.py <in.csv> <out.csv>
"""
import csv
import sys
from collections import defaultdict


def edit_distance_le_one(a: str, b: str) -> bool:
    """True if a and b differ by ≤1 char (same length only — substitutions)."""
    if a == b:
        return True
    if len(a) != len(b):
        return False
    diff = 0
    for x, y in zip(a, b):
        if x != y:
            diff += 1
            if diff > 1:
                return False
    return diff == 1


def cluster_serials(items: list[tuple[str, float]]) -> list[list[tuple[str, float]]]:
    """Greedy clustering: each new reading joins the first cluster whose
    representative is within edit distance ≤1. New cluster otherwise.
    items: [(serial, confidence), ...] — serial must be non-empty.
    """
    clusters: list[list[tuple[str, float]]] = []
    for serial, conf in items:
        joined = False
        for c in clusters:
            if edit_distance_le_one(serial, c[0][0]):
                c.append((serial, conf))
                joined = True
                break
        if not joined:
            clusters.append([(serial, conf)])
    return clusters


def pick_canonical(items: list[tuple[str, float]]) -> tuple[str, list[str]]:
    """Return (canonical_serial, rejected_serials).
    Strategy: cluster by edit distance ≤1. Score per cluster = (count,
    total_confidence). Count dominates so a 2-photo cluster beats a
    1-photo cluster regardless of confidence — handles the common case
    of ground_truth.csv where confidence isn't recorded. Confidence
    only breaks ties between same-size clusters.
    Within the winning cluster, the highest-confidence reading becomes
    canonical (or first reading on confidence tie).
    """
    if not items:
        return "", []
    clusters = cluster_serials(items)
    # Score: (count, sum_confidence) — sorted descending
    scored = [(len(cl), sum(c for _, c in cl), cl) for cl in clusters]
    scored.sort(key=lambda x: (x[0], x[1]), reverse=True)
    winning = scored[0][2]
    winning.sort(key=lambda x: x[1], reverse=True)
    canonical = winning[0][0]
    rejected_set: set[str] = set()
    for _, _, cl in scored[1:]:
        for s, _ in cl:
            rejected_set.add(s)
    for s, _ in winning[1:]:
        if s != canonical:
            rejected_set.add(s)
    return canonical, sorted(rejected_set)


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: dedupe_serials.py <in.csv> <out.csv>", file=sys.stderr)
        return 1

    in_path = sys.argv[1]
    out_path = sys.argv[2]

    # collect: pc_no → [(serial, confidence, filename), ...]
    by_pc: dict[str, list[tuple[str, float, str]]] = defaultdict(list)
    missing_pc = 0

    with open(in_path, encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            pc = row.get("pc_no", "")
            if not pc:
                missing_pc += 1
                continue
            serial = row.get("serial_no", "")
            try:
                conf = float(row.get("mean_confidence", "0") or 0)
            except ValueError:
                conf = 0.0
            by_pc[pc].append((serial, conf, row.get("filename", "")))

    # write output
    with open(out_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "pc_no", "photo_count", "canonical_serial",
            "rejected_serials", "best_confidence", "filenames",
        ])

        def key(k: str):
            try:
                return (0, int(k))
            except ValueError:
                return (1, k)

        for pc in sorted(by_pc.keys(), key=key):
            photos = by_pc[pc]
            serial_items = [(s, c) for s, c, _ in photos if s]
            canonical, rejected = pick_canonical(serial_items)
            best_conf = max((c for _, c, _ in photos), default=0.0)
            filenames = [fn for _, _, fn in photos]
            writer.writerow([
                pc,
                len(photos),
                canonical,
                ";".join(rejected),
                f"{best_conf:.3f}",
                ";".join(filenames),
            ])

    total_pcs = len(by_pc)
    with_serial = sum(1 for items in by_pc.values()
                      if any(s for s, _, _ in items))
    print(f"Photos missing PC No.: {missing_pc}")
    print(f"Unique PCs: {total_pcs}")
    print(f"PCs with >=1 serial reading: {with_serial}")
    print(f"Output: {out_path}")
    return 0


# ---------------- inline tests (run via: python dedupe_serials.py --self-test) ----------------

def _self_test() -> int:
    assert edit_distance_le_one("ABCDEF1", "ABCDEF1")
    assert edit_distance_le_one("ABCDEF1", "ABCDEF2")
    assert edit_distance_le_one("C7WS9M2", "C7WS9N2")  # M↔N
    assert edit_distance_le_one("614ZFL2", "6L4ZFL2")  # 1↔L
    assert not edit_distance_le_one("ABCDEF1", "ABCDEF12")  # different length
    assert not edit_distance_le_one("ABCDEF1", "ABCDXY1")  # 2 diffs

    # PC 303 case (with confidence): 21GG1F3 vs C7WS9M2 vs C7WS9N2
    # C7WS9M2/N2 cluster together (size 2), 21GG1F3 alone (size 1)
    items = [("21GG1F3", 0.6), ("C7WS9M2", 0.8), ("C7WS9N2", 0.7)]
    canonical, rejected = pick_canonical(items)
    assert canonical == "C7WS9M2", f"got {canonical}"
    assert "21GG1F3" in rejected
    assert "C7WS9N2" in rejected

    # Same case but all confidence=0 (ground_truth.csv path) — cluster size wins
    items = [("21GG1F3", 0.0), ("C7WS9M2", 0.0), ("C7WS9N2", 0.0)]
    canonical, rejected = pick_canonical(items)
    assert canonical == "C7WS9M2", f"got {canonical} (expected size-2 cluster to win)"
    assert "21GG1F3" in rejected

    # All same reading → no rejected
    canonical, rejected = pick_canonical([("ABC1234", 0.9), ("ABC1234", 0.8)])
    assert canonical == "ABC1234"
    assert rejected == []

    # Single reading
    canonical, rejected = pick_canonical([("XYZ7890", 0.5)])
    assert canonical == "XYZ7890"
    assert rejected == []

    # Empty
    canonical, rejected = pick_canonical([])
    assert canonical == ""
    assert rejected == []

    print("All self-tests passed.")
    return 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(_self_test())
    sys.exit(main())
