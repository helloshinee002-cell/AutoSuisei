#!/usr/bin/env python3
"""
Long-running PaddleOCR worker.

Reads one image path per line from stdin, runs PaddleOCR + the same
No / Serial extraction as bulk_extract.py, emits one JSON line per
input on stdout. Loads the model once at startup so the watch-folder
flow doesn't pay a 1-2s warm-up cost per image.

Usage (from Qt QProcess):
    python scripts/ocr_worker.py [--category=pc[,monitor,accessory,donate]]

`--category` accepts a comma-separated list. With one category the worker
reads each image as that type. With several, it runs every ticked category's
pipeline per image and keeps the best-scoring result (best-of-N) — for a
folder that mixes asset types.

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

_VALID_CATS = ("pc", "monitor", "accessory", "donate")


def emit(obj: dict) -> None:
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def parse_categories() -> list[str]:
    """Read `--category=pc,monitor,...` (CSV) from argv → ordered, de-duped
    list of valid categories. Defaults to ['pc']. A single value still works
    (backward compatible with the old single-category protocol)."""
    for a in sys.argv[1:]:
        if a.startswith("--category="):
            out: list[str] = []
            for c in a.split("=", 1)[1].lower().split(","):
                c = c.strip()
                if not c:
                    continue
                if c in _VALID_CATS:
                    if c not in out:
                        out.append(c)
                else:
                    print(f"unknown category={c}; skipped", file=sys.stderr)
            if out:
                return out
            print("no valid --category given; using 'pc'", file=sys.stderr)
    return ["pc"]


def process_one(engine, img: Path, meta: dict, category: str) -> dict:
    """Run one category's full pipeline on `img` and return its result dict
    (the emit payload minus the "event" key). Raises on OCR failure so the
    caller can decide whether to fall back to another category.

    Logic per category is identical to bulk_extract — barcode-first Serial for
    pc/donate/monitor, sticker-fusion No. for monitor/donate."""
    if category == "donate":
        raw, joined, mean_conf, line_count = ocr_donate(engine, img)
    else:
        raw = None
        joined, mean_conf, line_count = ocr_with_rotation(engine, img, category)

    serial = extract_serial(joined, category)
    # Barcode-first Serial (pc + donate + monitor) — Dell barcode แม่นกว่า OCR
    # (laptop=Data Matrix, desktop=Code128 ZBar, monitor=CN serial reformat;
    # แก้ O↔0); อ่านไม่ออก → คง OCR. (accessory ตัด)
    serial_source = "ocr"
    if category in ("pc", "donate", "monitor"):
        bc = read_serial_barcode(img, category)
        if bc:
            serial, serial_source = bc, "barcode"

    if category == "donate":
        # DonateMore: เลข typed/printed → ใช้เลย + ข้าม model/Tesseract (เร็ว);
        # ไม่เจอ = สติกเกอร์เขียนมือไทย → fusion model+crop.
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
        # เลขกระดาษขาว: fusion model + crop (RapidOCR full-image อ่านไม่ได้)
        pc_no = (read_monitor_sticker_no(engine, img)
                 or extract_pc_no(joined, meta["pc_range"]))
    else:
        pc_no = extract_pc_no(joined, meta["pc_range"])

    return {
        "filename": img.name,
        "path": str(img),
        "photo_index": meta["photo_index"],
        "pc_no": pc_no,
        "serial_no": serial,
        "serial_source": serial_source,
        "category": category,
        "batch_id": meta["batch_id"],
        "photo_date": meta["photo_date"],
        "pc_range": meta["pc_range"],
        "mean_confidence": mean_conf,
        "line_count": line_count,
    }


def _score(r: dict) -> tuple:
    """Best-of ranking key — higher wins. Mirrors ocr_with_rotation's
    (has_serial, has_no, conf), with two tiebreaks above confidence:

      1. a CN- monitor serial beats anything else. A monitor sticker also
         carries a 7-char Dell Service Tag that the `pc` pipeline happily
         decodes off its Code128 barcode; for a monitor image we must never
         let that Service Tag win over the real CN serial. CN only ever comes
         from the monitor pipeline (PCs have 7-char tags, accessories numeric),
         so this lights up only for actual monitor reads — safe by construction.
      2. a decoded barcode beats OCR'd text (barcode is more trustworthy)."""
    s = r.get("serial_no", "")
    return (
        1 if s else 0,
        1 if s.startswith("CN-") else 0,
        1 if r.get("serial_source") == "barcode" else 0,
        1 if r.get("pc_no") else 0,
        r.get("mean_confidence", 0.0),
    )


def process_best(engine, img: Path, meta: dict, categories: list[str]) -> dict:
    """Run each ticked category and return the best-scoring result. One
    category = no overhead. Raises only if EVERY category failed."""
    if len(categories) == 1:
        return process_one(engine, img, meta, categories[0])
    results: list[dict] = []
    errors: list[str] = []
    for cat in categories:
        try:
            results.append(process_one(engine, img, meta, cat))
        except Exception as e:  # noqa: BLE001
            errors.append(f"{cat}: {e}")
    if not results:
        raise RuntimeError("; ".join(errors) or "all categories failed")
    return max(results, key=_score)


def main() -> int:
    categories = parse_categories()
    try:
        from rapidocr_onnxruntime import RapidOCR
    except ImportError as e:
        emit({"event": "fatal", "error": f"rapidocr import: {e}"})
        return 2

    cats_str = ",".join(categories)
    print(f"ocr_worker: loading PaddleOCR (categories={cats_str})…",
          file=sys.stderr)
    t0 = time.time()
    engine = RapidOCR()
    print(f"ocr_worker: model ready in {time.time()-t0:.1f}s", file=sys.stderr)
    emit({"event": "ready", "load_time": time.time() - t0,
          "categories": categories})

    for line in sys.stdin:
        path_str = line.strip()
        if not path_str or path_str.upper() == "QUIT":
            break
        img = Path(path_str)
        if not img.is_file():
            emit({"event": "error", "filename": path_str,
                  "error": "not a file"})
            continue

        meta = parse_filename(img.name)
        try:
            result = process_best(engine, img, meta, categories)
        except Exception as e:  # noqa: BLE001
            emit({"event": "error", "filename": img.name,
                  "path": str(img), "error": str(e)})
            continue

        emit({"event": "result", **result})

    print("ocr_worker: shutting down", file=sys.stderr)
    return 0


def _selfcheck() -> int:
    """`python ocr_worker.py --selfcheck` — verify the best-of ranking without
    needing OCR / a model. Fails loudly if _score's ordering ever breaks."""
    none = {"serial_no": "", "pc_no": "", "mean_confidence": 0.9}
    ocr = {"serial_no": "ABC", "serial_source": "ocr",
           "pc_no": "", "mean_confidence": 0.5}
    barcode = {"serial_no": "ABC", "serial_source": "barcode",
               "pc_no": "", "mean_confidence": 0.1}
    assert _score(ocr) > _score(none), "any Serial must beat no Serial"
    assert _score(barcode) > _score(ocr), "barcode tiebreak must beat OCR+conf"
    with_no = {"serial_no": "X", "serial_source": "ocr",
               "pc_no": "1", "mean_confidence": 0.4}
    hi_conf = {"serial_no": "X", "serial_source": "ocr",
               "pc_no": "", "mean_confidence": 0.9}
    assert _score(with_no) > _score(hi_conf), "having a No. must beat conf"
    assert max([none, ocr, barcode], key=_score) is barcode
    # monitor CN serial must outrank a Service Tag even when the Service Tag was
    # read off a barcode and has higher confidence (best-of-N: a monitor image
    # also runs the pc pipeline, which decodes the 7-char Service Tag).
    cn_ocr = {"serial_no": "CN-0JF27G-FCC00-7AM-CRPB-A04",
              "serial_source": "ocr", "pc_no": "10", "mean_confidence": 0.5}
    stag_bc = {"serial_no": "27W2NG2", "serial_source": "barcode",
               "pc_no": "110", "mean_confidence": 0.97}
    assert _score(cn_ocr) > _score(stag_bc), "CN must beat Service Tag"
    assert max([stag_bc, cn_ocr], key=_score) is cn_ocr
    assert parse_categories.__name__  # smoke: importable
    print("ocr_worker selfcheck: OK")
    return 0


if __name__ == "__main__":
    if "--selfcheck" in sys.argv:
        sys.exit(_selfcheck())
    sys.exit(main())
