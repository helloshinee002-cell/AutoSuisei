#!/usr/bin/env python3
"""
PaddleOCR sidecar using rapidocr-onnxruntime.

Modes:
    python ocr_paddle.py <image_path>
        One-shot: OCR single image, print JSON, exit.

    python ocr_paddle.py --repl
        Persistent: read image paths from stdin (one per line),
        write one JSON per line to stdout. Model loads once.
        End with EOF or empty line.

Output JSON shape:
    {
      "filename": "...",
      "joined_text": "line1\nline2\n...",
      "mean_confidence": 0.82,
      "line_count": 42
    }
    (bounding boxes excluded for compact output; add --boxes to include.)
"""
import json
import sys
from pathlib import Path


def ocr_one(engine, image_path: str, include_boxes: bool = False) -> dict:
    if not Path(image_path).exists():
        return {"filename": image_path, "error": "file not found"}
    try:
        result, _elapsed = engine(image_path)
    except Exception as e:  # noqa: BLE001
        return {"filename": image_path, "error": f"ocr failed: {e}"}

    lines = []
    confs = []
    for box, text, score in (result or []):
        confs.append(float(score))
        if include_boxes:
            lines.append({"text": text, "confidence": float(score),
                          "box": [[float(x), float(y)] for x, y in box]})
        else:
            lines.append({"text": text, "confidence": float(score)})

    mean_conf = sum(confs) / len(confs) if confs else 0.0
    return {
        "filename": image_path,
        "joined_text": "\n".join(ln["text"] for ln in lines),
        "mean_confidence": mean_conf,
        "line_count": len(lines),
        "lines": lines,
    }


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "missing arguments"}), file=sys.stderr)
        return 1

    include_boxes = "--boxes" in args
    if include_boxes:
        args.remove("--boxes")

    try:
        from rapidocr_onnxruntime import RapidOCR
    except ImportError as e:
        print(json.dumps({"error": f"rapidocr import: {e}"}), file=sys.stderr)
        return 2

    engine = RapidOCR()

    if args[0] == "--repl":
        # Signal ready so caller can synchronize
        sys.stderr.write("READY\n")
        sys.stderr.flush()
        for raw in sys.stdin:
            path = raw.strip()
            if not path:
                continue
            if path == "__quit__":
                break
            result = ocr_one(engine, path, include_boxes)
            sys.stdout.write(json.dumps(result, ensure_ascii=False) + "\n")
            sys.stdout.flush()
        return 0

    # One-shot mode
    result = ocr_one(engine, args[0], include_boxes)
    sys.stdout.write(json.dumps(result, ensure_ascii=False))
    return 0 if "error" not in result else 2


if __name__ == "__main__":
    sys.exit(main())
