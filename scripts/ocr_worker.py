#!/usr/bin/env python3
"""
Long-running PaddleOCR worker.

Reads one image path per line from stdin, runs PaddleOCR + the same
PC No / Serial extraction as bulk_extract.py, emits one JSON line per
input on stdout. Loads the model once at startup so the watch-folder
flow doesn't pay a 1-2s warm-up cost per image.

Protocol:
    stdin  : <abs_path>\n  (one image per line; EOF or "QUIT" to exit)
    stdout : {"event":"ready"}                       once at startup
             {"event":"result","filename":..., ...}  per image
             {"event":"error","filename":..., ...}   on per-image failure

stderr is human-readable progress / warnings.

Usage (from Qt QProcess):
    python scripts/ocr_worker.py
"""
import json
import sys
import time
from pathlib import Path

# Import the regex helpers from bulk_extract so the worker stays in sync
# with whatever the batch script does — no duplicated parser logic.
_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE))
from bulk_extract import (  # noqa: E402
    extract_pc_no,
    extract_serial,
    parse_filename,
)


def emit(obj: dict) -> None:
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def main() -> int:
    try:
        from rapidocr_onnxruntime import RapidOCR
    except ImportError as e:
        emit({"event": "fatal", "error": f"rapidocr import: {e}"})
        return 2

    print("ocr_worker: loading PaddleOCR…", file=sys.stderr)
    t0 = time.time()
    engine = RapidOCR()
    print(f"ocr_worker: model ready in {time.time()-t0:.1f}s", file=sys.stderr)
    emit({"event": "ready", "load_time": time.time() - t0})

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
            result, _ = engine(str(img))
        except Exception as e:  # noqa: BLE001
            emit({"event": "error", "filename": img.name,
                  "path": str(img), "error": str(e)})
            continue

        confs = [float(score) for (_box, _text, score) in (result or [])]
        mean_conf = sum(confs) / len(confs) if confs else 0.0
        joined = "\n".join(text for (_box, text, _score) in (result or []))

        meta = parse_filename(img.name)
        pc_no = extract_pc_no(joined, meta["pc_range"])
        serial = extract_serial(joined)

        emit({
            "event": "result",
            "filename": img.name,
            "path": str(img),
            "photo_index": meta["photo_index"],
            "pc_no": pc_no,
            "serial_no": serial,
            "batch_id": meta["batch_id"],
            "photo_date": meta["photo_date"],
            "pc_range": meta["pc_range"],
            "mean_confidence": mean_conf,
            "line_count": len(confs),
        })

    print("ocr_worker: shutting down", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
