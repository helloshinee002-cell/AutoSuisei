#!/usr/bin/env python3
"""Train YOLOv8n sticker-digit detector (class 0-9) on the synthetic dataset, export ONNX.

ใช้ training venv (มี ultralytics/torch):
    scripts/train/.venv/Scripts/python.exe scripts/train/train_digit.py [--epochs 60]

ผลลัพธ์: models/sticker_digit.onnx (รัน inference ด้วย onnxruntime ใน stack เดิม — ไม่ต้อง torch)
"""
import argparse
import shutil
from pathlib import Path

from ultralytics import YOLO

HERE = Path(__file__).resolve().parent
DATA = HERE / "dataset" / "data.yaml"
MODELS = HERE.parent.parent / "models"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--epochs", type=int, default=60)
    ap.add_argument("--imgsz", type=int, default=640)
    ap.add_argument("--batch", type=int, default=16)
    ap.add_argument("--weights", default="yolov8n.pt")  # transfer จาก COCO
    ap.add_argument("--workers", type=int, default=6)
    args = ap.parse_args()

    model = YOLO(args.weights)
    model.train(
        data=str(DATA), epochs=args.epochs, imgsz=args.imgsz, batch=args.batch,
        device="cpu", project=str(HERE / "runs"), name="digit", exist_ok=True,
        cache="disk", workers=args.workers,  # cache='ram' ทำ OOM (kill epoch 21) → ใช้ disk
        # augmentation ให้ทนภาพถ่ายมือถือจริง (มุม/แสง/scale)
        degrees=8, translate=0.1, scale=0.5, perspective=0.0005,
        hsv_v=0.4, hsv_s=0.4, mosaic=0.6, fliplr=0.0,  # ห้าม flip — เลขกลับด้านผิด
    )

    best = HERE / "runs" / "digit" / "weights" / "best.pt"
    # simplify=False: simplify ดึง onnxslim ผ่านเน็ต (offline → fail); กราฟไม่ simplify ก็รันได้ปกติ
    onnx_path = YOLO(str(best)).export(format="onnx", imgsz=args.imgsz, opset=12, simplify=False)
    MODELS.mkdir(parents=True, exist_ok=True)
    dst = MODELS / "sticker_digit.onnx"
    shutil.copyfile(onnx_path, dst)
    print("exported ONNX →", dst)


if __name__ == "__main__":
    main()
