#!/usr/bin/env python3
"""Dump raw PaddleOCR text for one image (debugging)."""
import sys
from pathlib import Path

if len(sys.argv) < 2:
    print("usage: ocr_debug.py <image>")
    sys.exit(1)

from rapidocr_onnxruntime import RapidOCR

engine = RapidOCR()
result, _ = engine(str(Path(sys.argv[1])))
print("=== text lines (with confidence) ===")
for box, text, score in (result or []):
    s = float(score) if score else 0.0
    print(f"  [{s:.2f}] {text!r}")
print("\n=== joined ===")
print("\n".join(text for _b, text, _s in (result or [])))
