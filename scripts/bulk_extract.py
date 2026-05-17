#!/usr/bin/env python3
"""
Bulk-run PaddleOCR + asset extraction on a folder of images.

Usage:
    python bulk_extract.py <folder> <output.csv>

Outputs CSV with columns:
    photo_index,filename,pc_no,serial_no,batch_id,photo_date,pc_range,
    mean_confidence,line_count,warnings
"""
import csv
import re
import sys
import time
from pathlib import Path


# ---------- regex (mirror src/ocr/AssetExtractor.cpp) ----------

PC_NO_RE = re.compile(
    r"(?:^|[^A-Za-z])[Nn][Oo°][\.\-\s:]*([0-9]{1,4})\b"
)
# Phase 9.2 fallback: line ที่เป็น 2-3 digit ล้วน (sticker / dark Notepad)
PC_NO_STANDALONE_LINE_RE = re.compile(r"^\s*([0-9]{2,3})\s*$")
SERIAL_LABELED_RE = re.compile(
    r"(?:S\s*[/\\]?\s*N|SERVICE\s*TAG)\s*\)?\s*[:.]?\s*([A-Z0-9]{7})\b"
)
SERIAL_STANDALONE_RE = re.compile(r"\b([A-Z0-9]{7})\b")

# mirror src/ocr/AssetExtractor.cpp Phase 9.1 blocklist
SERIAL_BLOCKLIST = {
    "PASS1OF", "DISK0C1", "DRIVE00", "NTFSSIZ",
    "BOOTXOF", "ELAPSED", "REMOVE0", "FIXED00",
}
BATCH_RE = re.compile(r"\((\d+)\)")
DATE_RE = re.compile(r"_(\d{2})(\d{2})(\d{2})_")
PHOTO_IDX_RE = re.compile(r"_(\d+)\.(?:jpg|jpeg|png|bmp|tif|tiff|webp)$", re.I)
PC_RANGE_RE = re.compile(r"pc\s*(\d+\s*-\s*\d+)", re.I)


def extract_pc_no(text: str) -> str:
    m = PC_NO_RE.search(text)
    if m:
        return m.group(1)
    # fallback: standalone 2-3 digit line
    for line in text.splitlines():
        lm = PC_NO_STANDALONE_LINE_RE.match(line)
        if lm:
            return lm.group(1)
    return ""


def extract_serial(text: str) -> str:
    upper = text.upper()
    m = SERIAL_LABELED_RE.search(upper)
    if m and m.group(1) not in SERIAL_BLOCKLIST:
        return m.group(1)
    for m in SERIAL_STANDALONE_RE.finditer(upper):
        cand = m.group(1)
        alphas = sum(1 for c in cand if c.isalpha())
        digits = sum(1 for c in cand if c.isdigit())
        if alphas >= 3 and digits >= 2 and cand not in SERIAL_BLOCKLIST:
            return cand
    return ""


def parse_filename(name: str) -> dict:
    out = {"batch_id": "", "photo_date": "", "photo_index": 0, "pc_range": ""}
    if (m := BATCH_RE.search(name)):
        out["batch_id"] = m.group(1)
    if (m := DATE_RE.search(name)):
        out["photo_date"] = f"20{m.group(1)}-{m.group(2)}-{m.group(3)}"
    if (m := PHOTO_IDX_RE.search(name)):
        try:
            out["photo_index"] = int(m.group(1))
        except ValueError:
            pass
    if (m := PC_RANGE_RE.search(name)):
        out["pc_range"] = m.group(1)
    return out


def looks_like_image(p: Path) -> bool:
    return p.suffix.lower() in {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff", ".webp"}


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: bulk_extract.py <folder> <out.csv>", file=sys.stderr)
        return 1

    folder = Path(sys.argv[1])
    out_csv = Path(sys.argv[2])
    if not folder.is_dir():
        print(f"not a directory: {folder}", file=sys.stderr)
        return 2

    try:
        from rapidocr_onnxruntime import RapidOCR
    except ImportError as e:
        print(f"rapidocr import: {e}", file=sys.stderr)
        return 2

    images = sorted([p for p in folder.iterdir() if p.is_file() and looks_like_image(p)])
    print(f"Found {len(images)} images. Loading PaddleOCR…", file=sys.stderr)
    t0 = time.time()
    engine = RapidOCR()
    print(f"Model loaded in {time.time()-t0:.1f}s. Processing…", file=sys.stderr)

    with_pc = 0
    with_sn = 0
    with out_csv.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "photo_index", "filename", "pc_no", "serial_no",
            "batch_id", "photo_date", "pc_range",
            "mean_confidence", "line_count", "warnings",
        ])
        t1 = time.time()
        for i, img in enumerate(images, 1):
            try:
                result, _ = engine(str(img))
            except Exception as e:  # noqa: BLE001
                writer.writerow([0, img.name, "", "", "", "", "", 0, 0, f"OCR error: {e}"])
                continue

            confs = [float(score) for (_box, _text, score) in (result or [])]
            mean_conf = sum(confs) / len(confs) if confs else 0.0
            joined = "\n".join(text for (_box, text, _score) in (result or []))

            pc_no = extract_pc_no(joined)
            serial = extract_serial(joined)
            meta = parse_filename(img.name)

            warnings_list = []
            if not pc_no:
                warnings_list.append("PC No. not found")
            if not serial:
                warnings_list.append("Serial not found")

            if pc_no:
                with_pc += 1
            if serial:
                with_sn += 1

            writer.writerow([
                meta["photo_index"], img.name, pc_no, serial,
                meta["batch_id"], meta["photo_date"], meta["pc_range"],
                f"{mean_conf:.3f}", len(confs), "; ".join(warnings_list),
            ])

            if i % 10 == 0:
                elapsed = time.time() - t1
                rate = i / elapsed if elapsed > 0 else 0
                eta = (len(images) - i) / rate if rate > 0 else 0
                print(f"  [{i}/{len(images)}] PC#={with_pc} Serial={with_sn} "
                      f"rate={rate:.2f}/s ETA={eta:.0f}s",
                      file=sys.stderr)

    elapsed = time.time() - t1
    pc_rate = 100 * with_pc / len(images) if images else 0
    sn_rate = 100 * with_sn / len(images) if images else 0
    print(f"\n=== Summary ===", file=sys.stderr)
    print(f"Total: {len(images)} photos in {elapsed:.1f}s "
          f"({elapsed/len(images):.1f}s/photo)", file=sys.stderr)
    print(f"PC No. hit:  {with_pc:3d}/{len(images)} ({pc_rate:.1f}%)", file=sys.stderr)
    print(f"Serial hit:  {with_sn:3d}/{len(images)} ({sn_rate:.1f}%)", file=sys.stderr)
    print(f"CSV: {out_csv}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
