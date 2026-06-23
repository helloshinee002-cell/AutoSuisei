#!/usr/bin/env python3
"""
Bulk-run PaddleOCR + asset extraction on a folder of images.

Usage:
    python bulk_extract.py <folder> <output.csv> [--progress-json]
                                                  [--category=pc|monitor|accessory|donate]

Categories:
    pc        (default) Dell Service Tag 7-char parser (PC&Laptop)
    monitor   Dell S/N full format `CN-XXXXX-XXXXX-XXX-XXXX(-A00)?` (Dell monitor)
    accessory Flexible — S/N label, numeric-with-dashes, or 8-15 digit line
    donate    Running no. (สติกเกอร์ / Notepad "NO.x") + Dell Service Tag / wmic S/N
              (org/place-name reader removed 2026-06-21 — Thai handwriting OCR'd garbage)

Outputs CSV with columns:
    photo_index,filename,pc_no,serial_no,batch_id,photo_date,pc_range,
    mean_confidence,line_count,warnings
"""
import csv
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

# embeddable Python (build/embedded-python, python._pth) ไม่เติม script dir ลง sys.path
# อัตโนมัติ → ให้ sibling import เช่น `from sticker_digit import …` หาเจอเมื่อรันไฟล์นี้ตรงๆ
sys.path.insert(0, str(Path(__file__).resolve().parent))


# ---------- common regex (mirror src/ocr/AssetExtractor.cpp) ----------

PC_NO_RE = re.compile(
    r"(?:^|[^A-Za-z])[Nn][Oo°][\.\-\s:]*([0-9]{1,4})\b"
)
PC_NO_STANDALONE_LINE_RE = re.compile(r"^\s*([0-9]{2,3})\s*$")

BATCH_RE = re.compile(r"\((\d+)\)")
DATE_RE = re.compile(r"_(\d{2})(\d{2})(\d{2})_")
PHOTO_IDX_RE = re.compile(r"_(\d+)\.(?:jpg|jpeg|png|bmp|tif|tiff|webp)$", re.I)
PC_RANGE_RE = re.compile(r"(?:pc|laptop)\s*(\d+\s*-\s*\d+)", re.I)

# ---------- PC&Laptop serial (Dell Service Tag 7-char) ----------

SERIAL_LABELED_RE = re.compile(
    r"(?:S\s*[/\\]?\s*N|SERVICE\s*TAG)\s*\)?\s*[:.]?\s*([A-Z0-9]{7})\b"
)
SERIAL_STANDALONE_RE = re.compile(r"\b([A-Z0-9]{7})\b")
SERIAL_BLOCKLIST = {
    "PASS1OF", "DISK0C1", "DRIVE00", "NTFSSIZ",
    "BOOTXOF", "ELAPSED", "REMOVE0", "FIXED00",
}

# ---------- Monitor serial (Dell S/N full format CN-...-A00) ----------
# Format ที่เจอ:  CN-07C2R4-72872-2BD-A8MM  /  CN-0JF27G-FCC00-76M-AKNB-A00
# 5-6 hyphenated alphanumeric segments. The 1st segment is always "CN".
# Some photos are truncated (last segment cut off) → allow 3-5 segments
# after CN. Each segment 2-7 chars (limits accidental matches against
# long barcodes like CBA1000…086117).
MONITOR_SERIAL_RE = re.compile(
    r"\b(CN-[A-Z0-9]{4,7}-[A-Z0-9]{3,6}-[A-Z0-9]{2,5}"
    r"(?:-[A-Z0-9]{2,5})?(?:-[A-Z0-9]{2,4})?)\b",
)
# Krungsri asset barcode pattern — these lines interleave with the CN serial
# in OCR output and break naive `-\n` collapse. Strip them before parsing.
CBA_ASSET_BARCODE_LINE_RE = re.compile(r"^\s*CBA[0-9]+\s*$", re.MULTILINE)
# Express SVC code / phone numbers — 7+ pure-digit lines that interleave with
# the CN serial and corrupt the `-\n` collapse (e.g. "4211505398" between
# CN-03F27G- and CHAB-A00). Strip in monitor parser only.
NUMERIC_NOISE_LINE_RE = re.compile(r"^\s*\d{7,}\s*$", re.MULTILINE)

# ---------- Accessory serial (flexible) ----------
# 1) "S/N:" label + value (alphanumeric, dashes allowed, 5-20 chars)
ACCESSORY_LABELED_RE = re.compile(
    r"(?:S\s*[/\\]?\s*N|SERIAL(?:\s*NUMBER)?)"
    r"\s*\)?\s*[:.]?\s*([A-Z0-9][A-Z0-9\-]{4,19}[A-Z0-9])\b",
    re.IGNORECASE,
)
# 2) Numeric line with optional dashes — fallback when no label is present
#    (e.g. "20210202" stamped on card, or "261-535-477" Verifone style)
ACCESSORY_NUMERIC_LINE_RE = re.compile(r"^\s*(\d[\d\-]{6,18}\d)\s*$", re.MULTILINE)
# Skip krungsri-asset barcode (CBA1000…) — that's the bank's asset tag, not device serial
ACCESSORY_ASSET_BARCODE_RE = re.compile(r"\bCBA[0-9]{10,}\b")


def parse_range_bounds(hint: str) -> tuple[int, int]:
    """'301-400' → (301, 400). Returns (0, 0) on invalid input."""
    m = re.match(r"\s*(\d+)\s*-\s*(\d+)\s*$", hint or "")
    if not m:
        return (0, 0)
    return (int(m.group(1)), int(m.group(2)))


def extract_pc_no(text: str, range_hint: str = "") -> str:
    lo, hi = parse_range_bounds(range_hint)
    have_range = lo > 0 and hi >= lo

    def in_range(digits: str) -> bool:
        if not have_range:
            return True
        try:
            return lo <= int(digits) <= hi
        except ValueError:
            return False

    # Primary "no.NN" — iterate all matches, prefer in-range (Phase 9.5)
    first_primary = ""
    for m in PC_NO_RE.finditer(text):
        digits = m.group(1)
        if not first_primary:
            first_primary = digits
        if in_range(digits):
            return digits

    # Lone-digit fallback
    first_lone = ""
    for line in text.splitlines():
        lm = PC_NO_STANDALONE_LINE_RE.match(line)
        if not lm:
            continue
        digits = lm.group(1)
        if not first_lone:
            first_lone = digits
        if in_range(digits):
            return digits

    if first_primary:
        return first_primary
    return first_lone


def extract_serial_pc(text: str) -> str:
    """PC&Laptop: Dell Service Tag 7-char."""
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


# Match a line that starts a CN serial (one or more segments, optional trailing hyphen).
# Each segment 2-7 chars to avoid swallowing barcodes / express-SVC codes.
_CN_LINE_RE = re.compile(r"^CN-[A-Z0-9]{2,7}(?:-[A-Z0-9]{2,7})*-?$")
# Continuation lines: 1-3 segments of 2-7 chars each. Rejecting 8+ digit blocks
# avoids merging "Express SVC code" (e.g. 4211505398) into the serial.
_CN_CONT_RE = re.compile(r"^[A-Z0-9]{2,7}(?:-[A-Z0-9]{2,7}){0,3}-?$")


def _merge_cn_fragments(text: str) -> str:
    """Merge consecutive lines that look like CN-serial fragments.

    OCR sometimes emits the serial pieces as separate lines without
    trailing hyphens — e.g.
        CN-0MMK39
        72872-59P-
        CU1U-A00
    This walker rejoins them with '-' into 'CN-0MMK39-72872-59P-CU1U-A00'.
    """
    lines = text.split("\n")
    out: list[str] = []
    i = 0
    while i < len(lines):
        stripped = lines[i].strip()
        if _CN_LINE_RE.match(stripped):
            merged = [stripped.rstrip("-")]
            i += 1
            while i < len(lines):
                nxt = lines[i].strip()
                if _CN_CONT_RE.match(nxt) and not nxt.startswith("CBA"):
                    merged.append(nxt.rstrip("-"))
                    i += 1
                else:
                    break
            out.append("-".join(merged))
        else:
            out.append(lines[i])
            i += 1
    return "\n".join(out)


def extract_serial_monitor(text: str) -> str:
    """Monitor: full Dell S/N format CN-XXXXX-XXXXX-XXX-XXXX(-A00)?

    Never falls back to the 7-char Service Tag — that is intentional
    (user requirement 2026-05-18). Pre-processing:
        1. Strip krungsri asset-barcode lines (CBA1000…) that interleave
           with the CN serial in OCR output.
        2. Collapse `-\n` so CN segments span line breaks.
        3. Merge consecutive `[A-Z0-9-]+` lines that follow a `CN-` line
           (OCR sometimes drops the trailing hyphen).
    Tolerates truncated photos (3 segments after CN minimum).
    """
    upper = text.upper()
    stripped = CBA_ASSET_BARCODE_LINE_RE.sub("", upper)
    stripped = NUMERIC_NOISE_LINE_RE.sub("", stripped)
    collapsed = re.sub(r"-\s*\n\s*", "-", stripped)
    merged = _merge_cn_fragments(collapsed)
    m = MONITOR_SERIAL_RE.search(merged)
    if m:
        return m.group(1)
    return ""


def extract_serial_accessory(text: str) -> str:
    """Accessory: flexible parser.

    Tries in order:
        1. "S/N:" / "Serial:" label + value
        2. Numeric-with-dashes line (e.g. "261-535-477")
        3. Standalone 8-15 digit line (e.g. stamped "20210202")
    Skips the krungsri asset barcode CBA1000... (that's the bank's asset
    tag — every photo has it — not the device serial).
    """
    # 1) labeled
    m = ACCESSORY_LABELED_RE.search(text)
    if m:
        cand = m.group(1).upper()
        # ห้ามเป็น asset barcode
        if not ACCESSORY_ASSET_BARCODE_RE.match(cand):
            return cand

    # 2/3) numeric line — pick the longest non-asset-barcode candidate
    candidates: list[str] = []
    for m in ACCESSORY_NUMERIC_LINE_RE.finditer(text):
        cand = m.group(1)
        # Skip asset barcode (CBA prefix is letters so the numeric-only
        # regex won't match it directly, but a partial digit chunk might —
        # filter explicitly to be safe).
        if ACCESSORY_ASSET_BARCODE_RE.search(cand):
            continue
        candidates.append(cand)
    if candidates:
        # Prefer the longest; on tie, prefer with dashes (Verifone-style)
        candidates.sort(key=lambda s: (len(s), "-" in s), reverse=True)
        return candidates[0]
    return ""


def extract_serial(text: str, category: str) -> str:
    if category == "monitor":
        return extract_serial_monitor(text)
    if category == "accessory":
        return extract_serial_accessory(text)
    if category == "donate":
        # chassis "SERVICE TAG(S/N):" + screen wmic "SerialNumber" (ผ่อน alpha filter)
        return extract_serial_donate(text)
    return extract_serial_pc(text)


# ---------- Donate: org-name (Thai) reader REMOVED 2026-06-21 ----------
# user: "เอาตัวอ่านภาษาไทยออกไปเลย" — Tesseract `tha` อ่านชื่อโรงเรียนลายมือมั่ว.
# donate เหลือ No. (เลขสติกเกอร์) + Serial เท่านั้น. ตัว anchor เลข (locate_sticker_bbox /
# donate_fields_from_crop / _trailing_sticker_no / _THAI_LETTER_RE) ยังอยู่ — ใช้หา *ตัวเลข*
# ไม่ได้ output ตัวหนังสือไทย.


# เลขสติกเกอร์ = เลขรันต่อโรงเรียน (1/2/4/8/16…) อยู่ช่วง 1-99 → จับ 1-2 หลักที่ไม่ติดเลขอื่น.
# digit-boundary ตัด Express code (11 หลัก) + IO noise ("1010I"/"10101") ที่เป็น run ≥3 หลักทิ้ง,
# และยังจับเลขที่ติดอักษรไทย/latin มั่วได้ (เช่น "nnu4"→"4").
# ponytail: cap 2 หลัก ตัด false-positive เลข 3 หลักหลง (เช่น 445/812); ถ้าเลขสติกเกอร์เกิน 99 ค่อยขยายเป็น {1,3}
# ใช้ [0-9] ไม่ใช่ \d เพราะ Python \d จับเลขไทย (๐-๙) ด้วย — barcode มักอ่านเป็นเลขไทย = noise
_STICKER_NO_RE = re.compile(r"(?<![0-9])([0-9]{1,2})(?![0-9])")


def extract_pc_no_donate(text: str) -> str:
    """donate No. — เลขครุภัณฑ์ (จอ Notepad) หรือเลขสติกเกอร์ (1/2/4/8/16).

    1. ลอง `extract_pc_no` ก่อน — จับ "No.20" (จอ) + เลข 2-3 หลักทั้งบรรทัด ("16")
    2. ถ้าว่าง → จับเลขสติกเกอร์โดดๆ: ทิ้งบรรทัด SERVICE TAG ก่อน (กันเลขใน Service Tag เช่น
       6F10GL2 → "10" หลุดเข้ามา) แล้วเอา digit-run 1-3 หลักตัวแรก. Express code (run ยาว)
       ถูกตัดด้วย digit-boundary อยู่แล้ว — *ไม่อ่าน ปล่อยผ่าน*
    ponytail: best-effort — เลขเดี่ยวบนกระดาษขาว PaddleOCR อาจ detect ไม่เจอเลย → No. ว่าง
    ให้คนกรอกใน Review. ceiling: ถ้า OCR ตัด express code เป็นท่อน ≤3 หลักอาจหลุด (ยังไม่เจอในตัวอย่าง)
    """
    primary = extract_pc_no(text)
    if primary:
        return primary
    kept = [ln for ln in text.splitlines() if not SERIAL_LABELED_RE.search(ln.upper())]
    m = _STICKER_NO_RE.findall("\n".join(kept))
    return m[0] if m else ""


def _is_subseq(a: str, b: str) -> bool:
    """True ถ้า a เป็น subsequence ของ b (เรียงเดิม) — '2'⊆'20', '17'⊆'107'."""
    it = iter(b)
    return all(c in it for c in a)


def fuse_sticker_no(model_no: str, crop_no: str) -> str:
    """รวมผล YOLO digit-model + text-anchored crop ให้ได้เลขสติกเกอร์ที่ดีที่สุด.

    วัดจริงบน Photos-3-001: โมเดล **high-precision/low-recall** — เลขที่ detect ถูก แต่หล่นหลัก
    (เลข 1/5/9 บนสติกเกอร์ขาว detector มักไม่เจอ → "20"→"2"); ส่วน crop อ่าน 2 หลักครบแต่
    hallucinate เลขเดี่ยว (gt 8 → "29"). กฎ: ถ้า model เป็น subsequence ของ crop ที่ยาวกว่า
    แปลว่า model หล่นหลัก → ใช้ crop; ไม่งั้น crop เพี้ยน → ใช้ model. บน 31 เคสที่สองวิธีไม่ตรงกัน
    กฎนี้ถูก 24 (= ทุกเคสที่มีอย่างน้อยหนึ่งวิธีถูก) เทียบ crop-only 12 / model-only 12.
    ponytail: heuristic ชน ceiling 7 เคสที่ทั้งคู่ผิด (เลขหายจริง) → ปิด gap ด้วย fine-tune จริง (Phase B).
    """
    if model_no and crop_no and model_no != crop_no:
        if len(crop_no) > len(model_no) and _is_subseq(model_no, crop_no):
            return crop_no
        return model_no
    return model_no or crop_no


# DonateMore: เลขเป็น typed/printed (Notepad "NO.7" / สติกเกอร์ "New PC Donate 677") → OCR อ่านออกครบ
_PURE_NUM_LINE_RE = re.compile(r"(?m)^\s*([0-9]{1,3})\s*$")
_DONATE_KW_RE = re.compile(r"donate", re.I)


def extract_no_donate_explicit(text: str, range_hint: str = "") -> str:
    """เลข No. แบบ 'ระบุชัด' สำหรับ DonateMore (typed/printed) — มาก่อน fusion สติกเกอร์เขียนมือ.

    1) Notepad "NO.7"/"no.20"/"laptop no.63" → `PC_NO_RE` (สัญญาณแข็งสุด, typed text)
    2) สติกเกอร์ "New PC Donate 677"/"Laptop Donate 11" → บรรทัดเลขล้วน 1-3 หลัก เมื่อมีคำ "Donate"
       (กรองด้วย range hint ถ้ามี เช่น 'SN PC 1-990' → [1,990])
    คืน '' ถ้าไม่เข้าเงื่อนไข → ปล่อยให้ path สติกเกอร์ไทย (model/crop fusion) ทำงานต่อ
    → ไม่ regress Photos-3-001 (สติกเกอร์ไทยไม่มีคำ 'No.'/'Donate' ในข้อความ OCR).
    """
    m = PC_NO_RE.search(text)
    if m:
        return m.group(1)
    # สติกเกอร์ chassis: เลขมักอยู่ "หลัง" คำ Donate (เช่น 'Laptop Donate'\n...\n'11') — เลือกบรรทัด
    # เลขล้วน 1-3 หลักที่ใกล้คำ Donate ที่สุด โดยให้น้ำหนัก "บรรทัดหลัง Donate" ก่อน (กัน stray เช่น '256'
    # ที่อยู่ก่อนหน้า). กรอง range hint ถ้ามี (เช่น 'SN PC 1-990').
    lines = text.split("\n")
    donate_idx = next((i for i, ln in enumerate(lines) if _DONATE_KW_RE.search(ln)), -1)
    if donate_idx < 0:
        return ""
    lo, hi = parse_range_bounds(range_hint)
    cands = []  # (after_donate?, distance, value)
    for i, ln in enumerate(lines):
        nm = re.match(r"^\s*([0-9]{1,3})\s*$", ln)
        if not nm:
            continue
        val = nm.group(1)
        if lo > 0 and not (lo <= int(val) <= hi):
            continue
        cands.append((0 if i >= donate_idx else 1, abs(i - donate_idx), val))
    return min(cands)[2] if cands else ""


def _valid_dell_tag(tok: str) -> bool:
    """7-char alnum ที่มีทั้งตัวอักษรและตัวเลข (Dell tag) ไม่อยู่ใน blocklist."""
    return (len(tok) == 7 and any(c.isalpha() for c in tok)
            and any(c.isdigit() for c in tok) and tok not in SERIAL_BLOCKLIST)


def extract_serial_donate(text: str) -> str:
    """donate serial: Dell tag chassis (labeled) + wmic 'SerialNumber' จอ (7-char alnum).

    ต่าง extract_serial_pc: ผ่อน filter เป็น ≥1 alpha+≥1 digit (Dell tag เช่น B6W7103 มี 2 ตัวอักษร
    — เดิม ≥3 ตก) แต่ยังตัด express-code (ตัวเลขล้วน) + blocklist.
    screen wmic: anchor บรรทัด 'SerialNumber' → token 3 บรรทัดถัดไป (ข้าม 'DESKTOP-xxx' computer name
    ที่มาจาก first wmic call ที่ fail).
    """
    upper = text.upper()
    m = SERIAL_LABELED_RE.search(upper)
    if m and m.group(1) not in SERIAL_BLOCKLIST:
        return m.group(1)
    lines = upper.split("\n")
    for i, ln in enumerate(lines):
        if "SERIALNUMBER" not in ln:
            continue
        for nxt in lines[i + 1:i + 4]:
            if nxt.strip().startswith("DESKTOP"):
                continue
            for tok in re.findall(r"[A-Z0-9]{7}", nxt):
                if _valid_dell_tag(tok):
                    return tok
    for ln in lines:
        if ln.strip().startswith("DESKTOP"):
            continue
        for cm in SERIAL_STANDALONE_RE.finditer(ln):
            if _valid_dell_tag(cm.group(1)):
                return cm.group(1)
    return ""


def ocr_donate(engine, img_path):
    """donate: OCR รอบเดียว (ภาพตั้งตรง ไม่ต้องหมุน) — คืน (raw_result, joined, mean_conf, line_count).

    เก็บ raw_result (box+text+score) ไว้ทำ position-aware No. ([[sticker_no_from_boxes]]).
    """
    raw = engine(str(img_path))[0] or []
    joined = "\n".join(t for _b, t, _s in raw)
    confs = [float(s) for _b, _t, s in raw]
    mean_conf = sum(confs) / len(confs) if confs else 0.0
    return raw, joined, mean_conf, len(raw)


def sticker_no_from_boxes(result) -> str:
    """donate No. จากตำแหน่ง box — สติกเกอร์อยู่ฝั่งซ้ายเสมอ, Express code/Service Tag/IO-noise อยู่กลาง-ขวา.

    เอาเฉพาะ box ที่ center-x < 45% ของความกว้าง (proxy = ขอบขวาสุดของทุก box — เลี่ยง cv2.imread
    ที่เปิด unicode path ไม่ได้) แล้วดึงเลข 1-2 หลักจาก token ซ้ายสุดที่มีเลข. ใช้ digit-boundary จึง
    จับเลขที่ฟิวส์ตัวอักษรได้ (เช่น 'Hainn2' = "หมู่วิทยา 2" → '2'). Express/IO ฝั่งขวาถูกตัดด้วยเรขาคณิต.
    ponytail: left-position prior — สติกเกอร์ชุดนี้อยู่ซ้ายเสมอ; ถ้าสติกเกอร์ย้ายฝั่งต้องปรับ 0.45
    """
    if not result:
        return ""
    right_edge = max(p[0] for box, _t, _s in result for p in box)
    thresh = right_edge * 0.45
    left = []
    for box, txt, _s in result:
        cx = sum(p[0] for p in box) / 4.0
        if cx < thresh:
            left.append((cx, txt))
    for _cx, txt in sorted(left):  # ซ้ายสุดก่อน
        m = re.search(r"(?<![0-9])([0-9]{1,2})(?![0-9])", txt)
        if m:
            return m.group(1)
    return ""


# ---------- Donate: text-anchored sticker crop (ดัน No. → 95%+) ----------
# Thai letters/vowels/tones — ไม่รวมเลขไทย ๐-๙ (OCR ของ barcode มักออกมาเป็นเลขไทย = noise)
_THAI_LETTER_RE = re.compile(r"[ก-๏]")


def _imread_unicode(path):
    """cv2.imread ที่เปิด unicode/space path ได้ (imdecode). คืน None ถ้าอ่านไม่ได้."""
    import cv2
    import numpy as np
    try:
        data = np.fromfile(str(path), dtype=np.uint8)
        return cv2.imdecode(data, cv2.IMREAD_COLOR)
    except (OSError, ValueError):
        return None


def _tesseract_tsv_words(img_path):
    """รัน Tesseract (tha+eng) คืน word boxes [(text, left, top, w, h)].

    ใช้ `-c tessedit_create_tsv=1` (config file 'tsv' หายจาก install นี้) → เขียน `<base>.tsv`.
    copy ภาพไป ASCII temp ก่อน เพราะ tesseract เปิด unicode path ไม่ได้.
    """
    src = Path(img_path)
    tmp_img = ""
    tsv_path = ""
    try:
        fd, tmp_img = tempfile.mkstemp(suffix=(src.suffix or ".png"), prefix="stk_")
        os.close(fd)
        shutil.copyfile(src, tmp_img)
        base = tmp_img + "_o"
        tsv_path = base + ".tsv"
        subprocess.run(
            ["tesseract", tmp_img, base, "-l", "tha+eng", "--psm", "6",
             "-c", "tessedit_create_tsv=1"],
            capture_output=True, timeout=60,
        )
        words = []
        with open(tsv_path, encoding="utf-8") as f:
            for row in csv.DictReader(f, delimiter="\t"):
                t = (row.get("text") or "").strip()
                if not t:
                    continue
                try:
                    words.append((t, int(row["left"]), int(row["top"]),
                                  int(row["width"]), int(row["height"]),
                                  float(row.get("conf", 0) or 0)))
                except (ValueError, KeyError):
                    pass
        return words
    except (OSError, subprocess.SubprocessError):
        return []
    finally:
        for p in (tmp_img, tsv_path):
            if p and os.path.exists(p):
                try:
                    os.remove(p)
                except OSError:
                    pass


def locate_sticker_bbox(img_path, img_w):
    """หา bbox สติกเกอร์ จาก cluster ของ word ตัวอักษรไทย ฝั่งซ้าย. คืน (x0,y0,x1,y1) หรือ None.

    กัน 2 เคสพัง: (ก) ไทย-misread กระจายทั้งภาพ → cluster รอบ word ไทย**ใหญ่สุด** (สติกเกอร์ตัวใหญ่+แน่น),
    (ข) เจอไทยแค่ fragment จิ๋ว → conf filter + เช็ค bbox เล็กเกินแล้วคืน None (ให้ fallback ทำงาน).
    """
    words = _tesseract_tsv_words(img_path)
    img_h = max((tp + h for (_t, _l, tp, _w, h, _c) in words), default=0)
    thai = [(l, tp, w, h) for (t, l, tp, w, h, conf) in words
            if _THAI_LETTER_RE.search(t) and conf > 30 and (l + w / 2) < img_w * 0.55]
    if not thai:
        return None
    x0 = min(l for l, _, _, _ in thai)
    y0 = min(tp for _, tp, _, _ in thai)
    x1 = max(l + w for l, _, w, _ in thai)
    y1 = max(tp + h for _, tp, _, h in thai)
    bw, bh = x1 - x0, y1 - y0
    # size sanity: เล็กเกิน (fragment เดียว) หรือใหญ่เกิน (ไทย-misread กระจายทั้งภาพ) → anchor ไม่น่าเชื่อ
    # → คืน None ให้ fallback (RapidOCR position-aware) ทำงานแทน
    if bw < img_w * 0.04 or bw > img_w * 0.5 or (img_h and bh > img_h * 0.45):
        return None
    return (x0, y0, x1, y1)


def _trailing_sticker_no(text: str) -> str:
    """เลขรันสติกเกอร์ = เลข 1-2 หลักตัวท้ายของ**บรรทัดที่มีอักษรไทย** (school name ลงท้ายด้วยเลข).

    ผูกเลขกับบรรทัดชื่อโรงเรียน เพื่อไม่ไปหยิบเลขใน Service Tag/chassis ที่อยู่บรรทัดอื่น.
    """
    cands = []
    for line in text.splitlines():
        if _THAI_LETTER_RE.search(line):
            cands += re.findall(r"(?<![0-9])([0-9]{1,2})(?![0-9])", line)
    if cands:
        return cands[-1]
    nums = re.findall(r"(?<![0-9])([0-9]{1,2})(?![0-9])", text)
    return nums[-1] if nums else ""


def donate_fields_from_crop(img_path):
    """text-anchored: crop สติกเกอร์ (ยึดข้อความไทยเพื่อหา *ตำแหน่ง*) → re-OCR (psm 6) → คืน **เลข** (str).

    อ่านแค่เลขสติกเกอร์ — ไม่อ่านชื่อไทยแล้ว (org reader ถูกลบ 2026-06-21). Thai anchor ยังจำเป็น
    เพื่อ locate ป้าย. คืน '' ถ้าหา anchor ไม่ได้ — *ตั้งใจ* ให้ caller fallback ไป RapidOCR position-aware
    (พิสูจน์แล้วว่าแม่นกว่าการเดาจาก left-region crop ที่กว้าง/noisy). single-PSM กัน false-positive
    (multi-PSM vote ทดลองแล้วหยิบ garbage จาก psm 4/11 — precision แย่ลง).
    """
    import cv2
    im = _imread_unicode(img_path)
    if im is None:
        return ""
    H, W = im.shape[:2]
    bbox = locate_sticker_bbox(img_path, W)
    if not bbox:
        return ""
    x0, y0, x1, y1 = bbox
    pw, ph = int((x1 - x0) * 0.45), int((y1 - y0) * 0.7)
    crop = im[max(0, y0 - ph):min(H, y1 + ph), max(0, x0 - pw):min(W, x1 + pw)]
    if crop.size == 0:
        return ""
    crop = cv2.resize(crop, None, fx=2, fy=2, interpolation=cv2.INTER_CUBIC)
    tmp = ""
    try:
        fd, tmp = tempfile.mkstemp(suffix=".png", prefix="stkcrop_")
        os.close(fd)
        cv2.imwrite(tmp, crop)
        txt = subprocess.run(
            ["tesseract", tmp, "stdout", "-l", "tha+eng", "--psm", "6"],
            capture_output=True, text=True, encoding="utf-8",
            errors="replace", timeout=60,
        ).stdout
    except (OSError, subprocess.SubprocessError):
        return ""
    finally:
        if tmp and os.path.exists(tmp):
            try:
                os.remove(tmp)
            except OSError:
                pass
    return _trailing_sticker_no(txt)


def _white_sticker_crop(im):
    """หา bbox สติกเกอร์กระดาษขาว (สว่าง + low-sat + solid + bright-fraction) → crop + upscale; None ถ้าไม่เจอ.
    crop ตัด Dell label / RoHS "⑩" ออก (อยู่นอกสติกเกอร์) → เหลือเฉพาะเลขรันบนกระดาษขาว."""
    import cv2
    import numpy as np
    H, W = im.shape[:2]
    gray = cv2.cvtColor(im, cv2.COLOR_BGR2GRAY)
    sat = cv2.cvtColor(im, cv2.COLOR_BGR2HSV)[:, :, 1]
    _, th = cv2.threshold(gray, 200, 255, cv2.THRESH_BINARY)
    th = cv2.morphologyEx(th, cv2.MORPH_CLOSE, np.ones((21, 21), np.uint8))
    cnts, _ = cv2.findContours(th, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    img_area = H * W
    best, ba = None, 0
    for c in cnts:
        x, y, w, h = cv2.boundingRect(c)
        area = w * h
        if area < img_area * 0.01 or area > img_area * 0.5:
            continue
        if not (0.35 < w / max(h, 1) < 3.2):
            continue
        if cv2.contourArea(c) / max(area, 1) < 0.55:
            continue
        if (gray[y:y + h, x:x + w] > 200).mean() < 0.35:   # paper = mostly very-bright px (กัน metallic/label)
            continue
        if sat[y:y + h, x:x + w].mean() > 70:               # achromatic white
            continue
        if area > ba:
            best, ba = (x, y, w, h), area
    if not best:
        return None
    x, y, w, h = best
    pad = int(0.08 * max(w, h))
    crop = im[max(0, y - pad):min(H, y + h + pad), max(0, x - pad):min(W, x + w + pad)]
    if crop.size == 0:
        return None
    s = max(1.0, 900.0 / max(crop.shape[:2]))
    return cv2.resize(crop, None, fx=s, fy=s, interpolation=cv2.INTER_CUBIC) if s > 1 else crop


def read_monitor_sticker_no(engine, img_path) -> str:
    """Monitor No. = เลขรันบนสติกเกอร์กระดาษขาว — **fusion** 2 สัญญาณ (RapidOCR full-image อ่านไม่ได้:
    ไทยไม่ออก + ข้ามเลขเดี่ยวบนกระดาษขาว + RoHS "⑩"=10 หลอก):
      - sticker model (YOLO onnx, full image) → จับเลขเดี่ยว 1-9 ที่ PaddleOCR ข้าม
      - RapidOCR บน **crop กระดาษขาว** → จับ 2 หลัก printed (RoHS/Dell อยู่นอก crop)
    fuse ด้วย subsequence rule. วัดบน batch จริง **~42%** (เพดาน free/local; retrain โมเดลดันสูงภายหลัง — recurring-loop).
    คืน '' ถ้าทั้งคู่ว่าง → caller fallback `extract_pc_no`.
    """
    im = _imread_unicode(img_path)
    if im is None:
        return ""
    from sticker_digit import read_sticker_number
    model_no = read_sticker_number(im)
    crop_no = ""
    crop = _white_sticker_crop(im)
    if crop is not None:
        try:
            result, _ = engine(crop)
            joined = " ".join(t for (_b, t, _s) in (result or []))
            nums = _STICKER_NO_RE.findall(joined)
            crop_no = nums[-1] if nums else ""
        except Exception:  # noqa: BLE001
            crop_no = ""
    return fuse_sticker_no(model_no, crop_no)


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


def ocr_with_rotation(engine, img_path: Path, category: str) -> tuple[str, float, int]:
    """OCR with rotation fallback for sideways/upside-down photos.

    Tries 0° first. If extraction fails (no serial for monitor/accessory,
    or no PC No. anywhere), retries 90°/270°/180° and picks the best result
    (serial-found > pc-no-found > highest mean confidence).

    Returns (joined_text, mean_confidence, line_count).
    """
    import cv2

    def _score(text: str) -> tuple[int, int, float]:
        """(has_serial, has_pc_no, mean_conf) — higher wins."""
        has_sn = 1 if extract_serial(text, category) else 0
        has_pc = 1 if extract_pc_no(text) else 0
        return (has_sn, has_pc, 0.0)  # mean_conf filled below

    def _run(img_arr_or_path) -> tuple[str, float, int]:
        result, _ = engine(img_arr_or_path)
        text = "\n".join(t for (_b, t, _s) in (result or []))
        confs = [float(s) for (_b, _t, s) in (result or [])]
        mc = sum(confs) / len(confs) if confs else 0.0
        return text, mc, len(result or [])

    # อ่านภาพแบบ unicode-safe (รองรับ path ไทย/ช่องว่าง) — cv2.imread / RapidOCR(path) เปิด non-ASCII ไม่ได้
    # (เดิมส่ง str(path) เข้า engine → ภาพในโฟลเดอร์ชื่อไทยอ่านไม่ออก = ผลว่าง)
    img_arr = _imread_unicode(img_path)
    if img_arr is None:
        return "", 0.0, 0

    # 0° first
    text0, mc0, lc0 = _run(img_arr)
    s0 = (*_score(text0)[:2], mc0)
    # If both PC No. and serial found, no rotation needed
    if s0[0] == 1 and s0[1] == 1:
        return text0, mc0, lc0

    best_text, best_mc, best_lc = text0, mc0, lc0
    best_score = s0
    rotations = [
        (90, cv2.ROTATE_90_CLOCKWISE),
        (270, cv2.ROTATE_90_COUNTERCLOCKWISE),
        (180, cv2.ROTATE_180),
    ]
    for _angle, code in rotations:
        rotated = cv2.rotate(img_arr, code)
        text, mc, lc = _run(rotated)
        score = (*_score(text)[:2], mc)
        if score > best_score:
            best_text, best_mc, best_lc, best_score = text, mc, lc, score
            # Early exit if we found both PC No. and serial
            if score[0] == 1 and score[1] == 1:
                break
    return best_text, best_mc, best_lc


def parse_args() -> tuple[list[str], set[str], str]:
    positional: list[str] = []
    flags: set[str] = set()
    category = "pc"
    for a in sys.argv[1:]:
        if a.startswith("--category="):
            category = a.split("=", 1)[1].lower()
        elif a.startswith("--"):
            flags.add(a)
        else:
            positional.append(a)
    if category not in {"pc", "monitor", "accessory", "donate"}:
        print(f"unknown --category={category}; falling back to 'pc'", file=sys.stderr)
        category = "pc"
    return positional, flags, category


def main() -> int:
    args, flags, category = parse_args()
    progress_json = "--progress-json" in flags

    if len(args) < 2:
        print("usage: bulk_extract.py <folder> <out.csv> [--progress-json] "
              "[--category=pc|monitor|accessory|donate]",
              file=sys.stderr)
        return 1

    folder = Path(args[0])
    out_csv = Path(args[1])
    if not folder.is_dir():
        print(f"not a directory: {folder}", file=sys.stderr)
        return 2

    try:
        from rapidocr_onnxruntime import RapidOCR
    except ImportError as e:
        print(f"rapidocr import: {e}", file=sys.stderr)
        return 2

    images = sorted([p for p in folder.iterdir() if p.is_file() and looks_like_image(p)])
    print(f"Found {len(images)} images. Category={category}. Loading PaddleOCR…",
          file=sys.stderr)
    if progress_json:
        print(json.dumps({"event": "start", "total": len(images),
                          "category": category}), flush=True)
    t0 = time.time()
    engine = RapidOCR()
    print(f"Model loaded in {time.time()-t0:.1f}s. Processing…", file=sys.stderr)
    if progress_json:
        print(json.dumps({"event": "ready", "load_time": time.time() - t0}),
              flush=True)

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
                if category == "donate":
                    raw, joined, mean_conf, line_count = ocr_donate(engine, img)
                else:
                    raw = None
                    joined, mean_conf, line_count = ocr_with_rotation(engine, img, category)
            except Exception as e:  # noqa: BLE001
                writer.writerow([0, img.name, "", "", "", "", "", "", 0, 0, f"OCR error: {e}"])
                if progress_json:
                    print(json.dumps({
                        "event": "row", "i": i, "total": len(images),
                        "filename": img.name, "pc_no": "", "serial_no": "",
                        "error": str(e),
                    }), flush=True)
                continue

            meta = parse_filename(img.name)
            serial = extract_serial(joined, category)
            if category == "donate":
                # DonateMore: เลข typed/printed (Notepad "NO.x" / สติกเกอร์ "Donate n") OCR อ่านครบ
                # → ใช้เลย + ข้าม model/Tesseract (เร็ว ~3-4x). ถ้าไม่เจอ = สติกเกอร์เขียนมือไทย
                # (Photos-3-001) → fusion model + crop. (อ่านชื่อไทยถูกลบ — donate = No. + Serial)
                explicit = extract_no_donate_explicit(joined, meta["pc_range"])
                if explicit:
                    pc_no = explicit
                else:
                    # lazy import: model path ใช้ onnxruntime/cv2 เฉพาะ embedded python
                    from sticker_digit import read_sticker_number_path
                    model_no = read_sticker_number_path(str(img))
                    crop_no = donate_fields_from_crop(img)
                    # crop-side รวมทุกสัญญาณ OCR ก่อน fuse (เลข 2 หลักมักมาจาก fallback พวกนี้)
                    crop_side = (crop_no or sticker_no_from_boxes(raw)
                                 or extract_pc_no_donate(joined))
                    pc_no = fuse_sticker_no(model_no, crop_side)
            elif category == "monitor":
                # เลขกระดาษขาว: fusion model + crop (RapidOCR full-image อ่านไม่ได้) → ~42%; ว่าง → fallback parser
                pc_no = (read_monitor_sticker_no(engine, img)
                         or extract_pc_no(joined, meta["pc_range"]))
            else:
                pc_no = extract_pc_no(joined, meta["pc_range"])

            warnings_list = []
            if not pc_no:
                warnings_list.append("No. not found")
            if not serial:
                warnings_list.append("Serial not found")
            if pc_no:
                with_pc += 1
            if serial:
                with_sn += 1

            writer.writerow([
                meta["photo_index"], img.name, pc_no, serial,
                meta["batch_id"], meta["photo_date"], meta["pc_range"],
                f"{mean_conf:.3f}", line_count, "; ".join(warnings_list),
            ])

            if progress_json:
                print(json.dumps({
                    "event": "row",
                    "i": i,
                    "total": len(images),
                    "photo_index": meta["photo_index"],
                    "filename": img.name,
                    "pc_no": pc_no,
                    "serial_no": serial,
                    "batch_id": meta["batch_id"],
                    "photo_date": meta["photo_date"],
                    "pc_range": meta["pc_range"],
                    "mean_confidence": mean_conf,
                    "with_pc": with_pc,
                    "with_sn": with_sn,
                }), flush=True)

            if i % 10 == 0:
                elapsed = time.time() - t1
                rate = i / elapsed if elapsed > 0 else 0
                eta = (len(images) - i) / rate if rate > 0 else 0
                print(f"  [{i}/{len(images)}] No#={with_pc} Serial={with_sn} "
                      f"rate={rate:.2f}/s ETA={eta:.0f}s",
                      file=sys.stderr)

    elapsed = time.time() - t1
    pc_rate = 100 * with_pc / len(images) if images else 0
    sn_rate = 100 * with_sn / len(images) if images else 0
    if progress_json:
        print(json.dumps({
            "event": "done",
            "total": len(images),
            "with_pc": with_pc,
            "with_sn": with_sn,
            "elapsed_sec": elapsed,
            "category": category,
        }), flush=True)
    print(f"\n=== Summary (category={category}) ===", file=sys.stderr)
    print(f"Total: {len(images)} photos in {elapsed:.1f}s "
          f"({elapsed/len(images):.1f}s/photo)", file=sys.stderr)
    print(f"No. hit:  {with_pc:3d}/{len(images)} ({pc_rate:.1f}%)", file=sys.stderr)
    print(f"Serial hit:  {with_sn:3d}/{len(images)} ({sn_rate:.1f}%)", file=sys.stderr)
    print(f"CSV: {out_csv}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
