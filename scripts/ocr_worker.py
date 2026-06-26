#!/usr/bin/env python3
"""
Long-running PaddleOCR worker.

Reads one image path per line from stdin, runs PaddleOCR + the same
No / Serial extraction as bulk_extract.py, emits one JSON line per
input on stdout. Loads the model once at startup so the watch-folder
flow doesn't pay a 1-2s warm-up cost per image.

Usage (from Qt QProcess):
    python scripts/ocr_worker.py [--category=pc|monitor|accessory|donate]

Protocol:
    stdin  : <abs_path>\n  (one image per line; EOF or "QUIT" to exit)
    stdout : {"event":"ready"}                       once at startup
             {"event":"result","filename":..., ...}  per image
             {"event":"error","filename":..., ...}   on per-image failure

stderr is human-readable progress / warnings.
"""
import json
import sys
import time
from pathlib import Path

# Reuse the parser + rotation fallback from bulk_extract so the worker
# stays in sync — no duplicated logic.
_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE))
from bulk_extract import (  # noqa: E402
    donate_fields_from_crop,
    extract_no_donate_explicit,
    extract_pc_no,
    extract_pc_no_donate,
    extract_serial,
    fuse_sticker_no,
    ocr_donate,
    ocr_with_rotation,
    parse_filename,
    read_monitor_sticker_no,
    read_serial_barcode,
    sticker_no_from_boxes,
)


def emit(obj: dict) -> None:
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def parse_category() -> str:
    """Read `--category=pc|monitor|accessory|donate` from argv. Defaults to 'pc'."""
    for a in sys.argv[1:]:
        if a.startswith("--category="):
            cat = a.split("=", 1)[1].lower()
            if cat in {"pc", "monitor", "accessory", "donate"}:
                return cat
            print(f"unknown --category={cat}; using 'pc'", file=sys.stderr)
    return "pc"


def main() -> int:
    category = parse_category()
    try:
        from rapidocr_onnxruntime import RapidOCR
    except ImportError as e:
        emit({"event": "fatal", "error": f"rapidocr import: {e}"})
        return 2

    print(f"ocr_worker: loading PaddleOCR (category={category})…",
          file=sys.stderr)
    t0 = time.time()
    engine = RapidOCR()
    print(f"ocr_worker: model ready in {time.time()-t0:.1f}s", file=sys.stderr)
    emit({"event": "ready", "load_time": time.time() - t0,
          "category": category})

    for line in sys.stdin:
        path_str = line.strip()
        if not path_str or path_str.upper() == "QUIT":
            break
        img = Path(path_str)
        if not img.is_file():
            emit({"event": "error", "filename": path_str,
                  "error": "not a file"})
            continue

        try:
            if category == "donate":
                raw, joined, mean_conf, line_count = ocr_donate(engine, img)
            else:
                raw = None
                joined, mean_conf, line_count = ocr_with_rotation(
                    engine, img, category)
        except Exception as e:  # noqa: BLE001
            emit({"event": "error", "filename": img.name,
                  "path": str(img), "error": str(e)})
            continue

        meta = parse_filename(img.name)
        serial = extract_serial(joined, category)
        # Barcode-first Serial (pc + donate + monitor) — Dell barcode แม่นกว่า OCR (laptop=Data Matrix,
        # desktop/laptop=Code128 ZBar, monitor=CN serial reformat; แก้ O↔0); อ่านไม่ออก → คง OCR. (accessory ตัด)
        serial_source = "ocr"
        if category in ("pc", "donate", "monitor"):
            bc = read_serial_barcode(img, category)
            if bc:
                serial, serial_source = bc, "barcode"
        if category == "donate":
            # DonateMore: เลข typed/printed (Notepad/Donate sticker) → ใช้เลย + ข้าม model/Tesseract (เร็ว);
            # ไม่เจอ = สติกเกอร์เขียนมือไทย (Photos-3-001) → fusion model+crop. (อ่านชื่อไทยถูกลบ — No.+Serial)
            explicit = extract_no_donate_explicit(joined, meta["pc_range"])
            if explicit:
                pc_no = explicit
            else:
                from sticker_digit import read_sticker_number_path
                model_no = read_sticker_number_path(str(img))
                crop_no = donate_fields_from_crop(img)
                crop_side = (crop_no or sticker_no_from_boxes(raw)
                             or extract_pc_no_donate(joined))
                pc_no = fuse_sticker_no(model_no, crop_side)
        elif category == "monitor":
            # เลขกระดาษขาว: fusion model + crop (RapidOCR full-image อ่านไม่ได้) → ~42%
            pc_no = (read_monitor_sticker_no(engine, img)
                     or extract_pc_no(joined, meta["pc_range"]))
        else:
            pc_no = extract_pc_no(joined, meta["pc_range"])

        emit({
            "event": "result",
            "filename": img.name,
            "path": str(img),
            "photo_index": meta["photo_index"],
            "pc_no": pc_no,
            "serial_no": serial,
            "serial_source": serial_source,
            "batch_id": meta["batch_id"],
            "photo_date": meta["photo_date"],
            "pc_range": meta["pc_range"],
            "mean_confidence": mean_conf,
            "line_count": line_count,
        })

    print("ocr_worker: shutting down", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
