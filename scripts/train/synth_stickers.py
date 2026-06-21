#!/usr/bin/env python3
"""Synthetic sticker-number dataset generator for the donate digit detector.

เรนเดอร์สติกเกอร์ปลอม "โรงเรียน...​ <N>" (N=0-99) → composite ทับ background chassis จริง
(จาก Photos-3-001) → augment (scale/rotate/brightness/blur/noise) → YOLO dataset.
digit-box label เป๊ะเพราะเรารู้ว่าวาดเลขตรงไหน → ไม่ต้อง label มือ.

Output: scripts/train/dataset/{images,labels}/{train,val}/ + data.yaml

Run (embedded python มี PIL/cv2/numpy พอ):
    python scripts/train/synth_stickers.py [--n 2000] [--val 200] [--bg <dir>]
"""
import argparse
import os
import random
import sys
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
OUT = HERE / "dataset"
DEFAULT_BG = Path(r"C:/Users/hello/Downloads/Photos-3-001")

# ฟอนต์ไทย Windows หลายแบบ → กันโมเดล overfit ฟอนต์เดียว
FONT_CANDIDATES = [
    "C:/Windows/Fonts/tahoma.ttf", "C:/Windows/Fonts/tahomabd.ttf",
    "C:/Windows/Fonts/leelawad.ttf", "C:/Windows/Fonts/leelawui.ttf",
    "C:/Windows/Fonts/angsa.ttc", "C:/Windows/Fonts/cordia.ttc",
    "C:/Windows/Fonts/upcil.ttf", "C:/Windows/Fonts/Kanit-Light.ttf",
]
FONTS = [f for f in FONT_CANDIDATES if os.path.exists(f)]

# ชื่อโรงเรียนปลอม (distractor — โมเดลต้องโฟกัสเลข ไม่ใช่ตัวอักษร)
PREFIX = ["โรงเรียนบ้าน", "โรงเรียนวัด", "โรงเรียนบ้านวัด", "ร.ร.บ้าน", "โรงเรียน"]
WORDS = ["หมู่วิทยา", "บ้านทราย", "กล้วย", "หนองบัว", "ดอนแก้ว", "ทุ่งศรี", "โพธิ์ทอง",
         "ห้วยยาง", "เขาดิน", "คลองใหม่", "ศรีสมบูรณ์", "นาโพธิ์", "วิทยาคม"]


def rand_name():
    return random.choice(PREFIX) + random.choice(WORDS)


def render_sticker(number_str):
    """เรนเดอร์สติกเกอร์ขาว 2 บรรทัด (ชื่อ + เลข) → (RGBA PIL img, digit_boxes[(x,y,w,h,cls)])."""
    font_path = random.choice(FONTS)
    fsize = random.randint(34, 52)
    try:
        font = ImageFont.truetype(font_path, fsize)
    except OSError:
        font = ImageFont.truetype(FONTS[0], fsize)
    line1 = rand_name()
    line2_name = random.choice(WORDS)
    gap = random.randint(int(fsize * 0.3), int(fsize * 0.8))
    pad = int(fsize * random.uniform(0.5, 1.1))

    def tw(s):
        b = font.getbbox(s)
        return b[2] - b[0], b[3] - b[1]

    w1, h1 = tw(line1)
    namew, h2 = tw(line2_name + " ")
    # วัดความกว้างเลขแต่ละหลัก
    digit_w = [tw(d)[0] for d in number_str]
    line2_w = namew + sum(digit_w)
    lineh = max(h1, h2)
    content_w = max(w1, line2_w)
    W = content_w + 2 * pad
    H = lineh * 2 + gap + 2 * pad
    img = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    d = ImageDraw.Draw(img)
    ink = (random.randint(0, 60),) * 3 + (255,)  # ดำ→เทาอ่อน → เส้นบาง (เช่น '1') อ่านยากขึ้นบ้าง
    y1 = pad
    y2 = pad + lineh + gap
    d.text((pad, y1), line1, font=font, fill=ink)
    d.text((pad, y2), line2_name + " ", font=font, fill=ink)
    # วาดเลขทีละหลัก track box
    boxes = []
    x = pad + namew
    for dig, dw in zip(number_str, digit_w):
        d.text((x, y2), dig, font=font, fill=ink)
        # box รัดเลข (เผื่อขอบนิดหน่อย)
        boxes.append([x, y2, dw, h2, int(dig)])
        x += dw
    return img, boxes


def warp_boxes(boxes, M):
    """transform [x,y,w,h,cls] ด้วย affine M → axis-aligned bbox ใหม่."""
    out = []
    for x, y, w, h, c in boxes:
        pts = np.array([[x, y], [x + w, y], [x + w, y + h], [x, y + h]], dtype=np.float32)
        pts = cv2.transform(pts.reshape(-1, 1, 2), M).reshape(-1, 2)
        x0, y0 = pts[:, 0].min(), pts[:, 1].min()
        x1, y1 = pts[:, 0].max(), pts[:, 1].max()
        out.append([x0, y0, x1 - x0, y1 - y0, c])
    return out


def compose(bg_bgr, sticker, boxes, size=640):
    """paste สติกเกอร์ลง bg ฝั่งซ้าย + scale/rotate/glare → (img 640x640 BGR, boxes ในพิกัดภาพ).

    scale floor ต่ำ (0.18) + glare = เลขเล็ก/ถูกแสงล้างแบบภาพจริง → โมเดลเรียน digit ที่ legibility ต่ำ
    (จุดอ่อนเดิม: เลข 1/5/9 บนสติกเกอร์ขาวที่ detector มองไม่เห็น).
    """
    bg = cv2.resize(bg_bgr, (size, size))
    sw, sh = sticker.size
    # scale สติกเกอร์ ~18-46% ของภาพ — ลด floor ให้มีเคสเลขเล็กเหมือนจริง
    target_w = random.uniform(0.18, 0.46) * size
    s = target_w / sw
    nw, nh = max(8, int(sw * s)), max(8, int(sh * s))
    st = sticker.resize((nw, nh), Image.LANCZOS)
    b = [[x * s, y * s, w * s, h * s, c] for x, y, w, h, c in boxes]
    # ตำแหน่งวาง: ฝั่งซ้าย
    px = int(random.uniform(0.01, 0.12) * size)
    py = int(random.uniform(0.20, 0.72) * size)
    if py + nh > size:
        py = size - nh - 1
    # rotate สติกเกอร์เล็กน้อย (±8°) รอบจุดวาง
    ang = random.uniform(-8, 8)
    st_np = np.array(st)
    M = cv2.getRotationMatrix2D((nw / 2, nh / 2), ang, 1.0)
    st_rot = cv2.warpAffine(st_np, M, (nw, nh), flags=cv2.INTER_LINEAR,
                            borderValue=(0, 0, 0, 0))
    # boxes: rotate รอบ center ของ sticker แล้ว +offset (px,py)
    b = warp_boxes(b, M)
    b = [[x + px, y + py, w, h, c] for x, y, w, h, c in b]
    # glare: ไฮไลต์สว่างนุ่ม ๆ บนสติกเกอร์ (เลียนแสงสะท้อนกระดาษขาว — บางครั้งล้างเลขจางลง)
    st_rgb = st_rot[:, :, :3].astype(np.float32)
    if random.random() < 0.55:
        gy, gx = np.ogrid[:nh, :nw]
        cxg, cyg = random.uniform(0, nw), random.uniform(0, nh)
        rad = random.uniform(0.30, 0.75) * max(nw, nh)
        g = np.exp(-(((gx - cxg) ** 2 + (gy - cyg) ** 2) / (2 * rad * rad + 1e-6)))
        st_rgb = np.clip(st_rgb + (g * random.uniform(55, 135)).astype(np.float32)[..., None], 0, 255)
    # alpha composite
    alpha = (st_rot[:, :, 3:4].astype(np.float32)) / 255.0
    roi = bg[py:py + nh, px:px + nw].astype(np.float32)
    bg[py:py + nh, px:px + nw] = (st_rgb[:, :, ::-1] * alpha + roi * (1 - alpha)).astype(np.uint8)
    return bg, b


def augment(img):
    """brightness/contrast/blur/noise/jpeg เลียนภาพถ่ายมือถือ — เข้มขึ้นให้ legibility ใกล้ภาพจริง."""
    img = img.astype(np.float32)
    img = img * random.uniform(0.45, 1.25) + random.uniform(-35, 22)
    img = np.clip(img, 0, 255).astype(np.uint8)
    # blur: gaussian แรงขึ้น (เลขเล็กในภาพจริงเบลอ) + บางทีเป็น motion blur
    if random.random() < 0.65:
        k = random.choice([3, 5, 5, 7])
        img = cv2.GaussianBlur(img, (k, k), 0)
    if random.random() < 0.22:
        ksz = random.choice([5, 7, 9])
        ker = np.zeros((ksz, ksz), np.float32)
        ker[ksz // 2, :] = 1.0 / ksz            # แนวนอน (มือสั่น/แพน)
        if random.random() < 0.5:
            ker = ker.T
        img = cv2.filter2D(img, -1, ker)
    if random.random() < 0.45:
        n = np.random.normal(0, random.uniform(4, 14), img.shape).astype(np.float32)
        img = np.clip(img.astype(np.float32) + n, 0, 255).astype(np.uint8)
    if random.random() < 0.7:
        q = random.randint(35, 85)
        _, enc = cv2.imencode(".jpg", img, [cv2.IMWRITE_JPEG_QUALITY, q])
        img = cv2.imdecode(enc, cv2.IMREAD_COLOR)
    return img


def yolo_label(boxes, size):
    """[x,y,w,h,cls] (pixel) → บรรทัด YOLO 'cls cx cy w h' (normalized), กรอง box นอกภาพ."""
    lines = []
    for x, y, w, h, c in boxes:
        cx, cy = (x + w / 2) / size, (y + h / 2) / size
        nw, nh = w / size, h / size
        if 0 < cx < 1 and 0 < cy < 1 and nw > 0 and nh > 0:
            lines.append(f"{c} {cx:.6f} {cy:.6f} {nw:.6f} {nh:.6f}")
    return lines


def number_pool():
    """เลขรันโรงเรียน: ~40% หลักเดียว (1-9), ~60% สองหลัก (10-99) — เน้นสองหลักเพราะเป็นจุดอ่อน."""
    if random.random() < 0.40:
        return str(random.randint(1, 9))
    return str(random.randint(10, 99))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=2000)
    ap.add_argument("--val", type=int, default=200)
    ap.add_argument("--bg", default=str(DEFAULT_BG))
    ap.add_argument("--size", type=int, default=640)
    args = ap.parse_args()
    if not FONTS:
        print("no Thai fonts found", file=sys.stderr)
        return 1
    bgs = [p for p in Path(args.bg).iterdir()
           if p.suffix.lower() in {".jpg", ".jpeg", ".png"}]
    if not bgs:
        print(f"no background images in {args.bg}", file=sys.stderr)
        return 2
    print(f"fonts={len(FONTS)} backgrounds={len(bgs)} → gen {args.n} train + {args.val} val",
          file=sys.stderr)

    def bg_read(p):
        data = np.fromfile(str(p), dtype=np.uint8)
        return cv2.imdecode(data, cv2.IMREAD_COLOR)

    for split, count in (("train", args.n), ("val", args.val)):
        idir = OUT / "images" / split
        ldir = OUT / "labels" / split
        idir.mkdir(parents=True, exist_ok=True)
        ldir.mkdir(parents=True, exist_ok=True)
        made = 0
        tries = 0
        while made < count and tries < count * 3:
            tries += 1
            num = number_pool()
            sticker, boxes = render_sticker(num)
            bg = bg_read(random.choice(bgs))
            if bg is None:
                continue
            # ครึ่งขวาเป็นพื้นหลัง: สติกเกอร์จริงอยู่ซ้าย → ตัดทิ้งกัน pollution
            # (เก็บ grille/ports/SERVICE TAG ไว้เป็น distractor ให้โมเดลเรียนว่าเลขพวกนั้นไม่ใช่)
            bg = bg[:, int(bg.shape[1] * 0.42):]
            img, b = compose(bg, sticker, boxes, args.size)
            img = augment(img)
            lines = yolo_label(b, args.size)
            if not lines:
                continue
            name = f"{split}_{made:05d}"
            cv2.imwrite(str(idir / f"{name}.jpg"), img)
            (ldir / f"{name}.txt").write_text("\n".join(lines), encoding="utf-8")
            made += 1
            if made % 200 == 0:
                print(f"  {split}: {made}/{count}", file=sys.stderr)
        print(f"{split}: {made} images", file=sys.stderr)

    (OUT / "data.yaml").write_text(
        f"path: {OUT.as_posix()}\n"
        "train: images/train\nval: images/val\n"
        "nc: 10\n"
        "names: ['0','1','2','3','4','5','6','7','8','9']\n",
        encoding="utf-8")
    print(f"dataset → {OUT}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
