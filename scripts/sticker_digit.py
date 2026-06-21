#!/usr/bin/env python3
"""Inference: อ่านเลขสติกเกอร์ donate ด้วย YOLOv8 digit-detector (ONNX) ผ่าน onnxruntime.

โมเดลเทรนจาก scripts/train/ (synthetic) → models/sticker_digit.onnx. รันใน stack เดิม
(onnxruntime + cv2 + numpy ที่ embedded python มีอยู่แล้ว) — ไม่ต้อง torch/ultralytics.

API:
    read_sticker_number(img_bgr) -> str   # "16" / "4" / "" ถ้าไม่เจอ
    read_sticker_number_path(path) -> str # unicode-safe
"""
from pathlib import Path

import cv2
import numpy as np

_MODEL = Path(__file__).resolve().parent.parent / "models" / "sticker_digit.onnx"
_session = None
_input_name = None


def _get_session():
    global _session, _input_name
    if _session is None:
        if not _MODEL.exists():
            return None
        try:
            import onnxruntime as ort
            _session = ort.InferenceSession(
                str(_MODEL), providers=["CPUExecutionProvider"])
            _input_name = _session.get_inputs()[0].name
        except Exception:  # noqa: BLE001
            return None
    return _session


def _letterbox(img, size=512):
    h, w = img.shape[:2]
    r = min(size / h, size / w)
    nh, nw = int(round(h * r)), int(round(w * r))
    resized = cv2.resize(img, (nw, nh))
    canvas = np.full((size, size, 3), 114, np.uint8)
    top, left = (size - nh) // 2, (size - nw) // 2
    canvas[top:top + nh, left:left + nw] = resized
    return canvas


def read_sticker_number(img_bgr, conf=0.35, iou=0.5, imgsz=512) -> str:
    """detect เลข 0-9 → คืนเลขเรียงซ้าย→ขวา ('' ถ้าไม่มีโมเดล/ไม่เจอ).

    จับเฉพาะ digit ที่อยู่กลุ่มเดียวกับ digit ความมั่นใจสูงสุด (กัน stray digit ไกลๆ
    เช่นที่อาจหลุดจาก service tag) — สติกเกอร์มีเลขชุดเดียว.
    """
    sess = _get_session()
    if sess is None or img_bgr is None:
        return ""
    canvas = _letterbox(img_bgr, imgsz)
    blob = canvas[:, :, ::-1].transpose(2, 0, 1)[None].astype(np.float32) / 255.0
    out = sess.run(None, {_input_name: blob})[0]      # [1, 4+nc, N]
    out = np.squeeze(out, 0).T                         # [N, 4+nc]
    xywh = out[:, :4]
    scores = out[:, 4:]
    cls = scores.argmax(1)
    cf = scores.max(1)
    keep = cf > conf
    if not keep.any():
        return ""
    xywh, cls, cf = xywh[keep], cls[keep], cf[keep]
    boxes = np.column_stack([xywh[:, 0] - xywh[:, 2] / 2,
                             xywh[:, 1] - xywh[:, 3] / 2,
                             xywh[:, 2], xywh[:, 3]])
    idxs = cv2.dnn.NMSBoxes(boxes.tolist(), cf.tolist(), conf, iou)
    if len(idxs) == 0:
        return ""
    idxs = np.array(idxs).flatten()
    cx = xywh[idxs, 0]
    cy = xywh[idxs, 1]
    h = xywh[idxs, 3]
    digit = cls[idxs]
    cfk = cf[idxs]
    # กลุ่มเดียวกับ digit มั่นใจสุด: อยู่แถวเดียวกัน (|Δy| < 1.2*h) → เลขชุดเดียว
    a = int(np.argmax(cfk))
    same = np.abs(cy - cy[a]) < 1.2 * h[a]
    order = np.argsort(cx[same])
    return "".join(str(int(d)) for d in digit[same][order])


def read_sticker_number_path(img_path) -> str:
    """unicode-safe (cv2.imdecode)."""
    try:
        data = np.fromfile(str(img_path), dtype=np.uint8)
        im = cv2.imdecode(data, cv2.IMREAD_COLOR)
    except (OSError, ValueError):
        return ""
    return read_sticker_number(im) if im is not None else ""


if __name__ == "__main__":
    import sys
    for p in sys.argv[1:]:
        print(p, "->", repr(read_sticker_number_path(p)))
