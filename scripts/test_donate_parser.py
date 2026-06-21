#!/usr/bin/env python3
"""Self-check for the donate parser additions (no OCR needed — pure text logic).

Run: python scripts/test_donate_parser.py
Sample texts mirror real Tesseract `tha+eng` output captured from the
Train Donate images (school line carries leading OCR noise; Thai loses some
diacritics — that's expected, the Review tab is where a human fixes it).
"""
import sys
from pathlib import Path

# embeddable Python ไม่เติม script dir ลง sys.path → ให้ `import bulk_extract` หาเจอ
sys.path.insert(0, str(Path(__file__).resolve().parent))

from bulk_extract import (  # noqa: E402
    _clean_org_line,
    _trailing_sticker_no,
    extract_no_donate_explicit,
    extract_org_name,
    extract_pc_no,
    extract_pc_no_donate,
    extract_serial,
    extract_serial_donate,
    fuse_sticker_no,
    sticker_no_from_boxes,
)


def _box(cx):
    """4-corner box centered at x=cx (y irrelevant for the position heuristic)."""
    return [[cx - 20, 0], [cx + 20, 0], [cx + 20, 20], [cx - 20, 20]]


def main() -> int:
    # --- extract_org_name: screen (Notepad) — school line with leading "ee" noise ---
    screen = "SerialNumber\n7CY0DK2\nee            รร.วดบานคลวย\nNo.20\n"
    got = extract_org_name(screen)
    assert got.startswith("รร."), got
    assert "No" not in got, got  # must not bleed the "No.20" line in

    # --- extract_org_name: sticker — marker embedded in noisy line ---
    sticker = "โรงเรียนบาน เรรบ แพพ 15962266850\nSERVICE TAG(S/N): 7C33DK2\n"
    assert extract_org_name(sticker).startswith("โรงเรียน"), extract_org_name(sticker)

    # --- no Thai at all → empty ---
    assert extract_org_name("SerialNumber\n7CY0DK2\nNo.20\n") == ""

    # --- _clean_org_line trims leading latin noise ---
    assert _clean_org_line("ee   รร.วดบานคลวย") == "รร.วดบานคลวย"

    # --- donate serial reuses the pc Dell Service Tag parser ---
    assert extract_serial("SERVICE TAG(S/N): 7C33DK2", "donate") == "7C33DK2"
    assert extract_serial("SerialNumber\n7CY0DK2", "donate") == "7CY0DK2"
    # donate must NOT use the monitor CN-…-A00 parser
    assert extract_serial("CN-07C2R4-72872-2BD-A8MM", "donate") == ""

    # --- No. extraction unchanged ---
    assert extract_pc_no("No.20") == "20"
    assert extract_pc_no("No.19") == "19"

    # --- donate No.: sticker number (texts mirror real RapidOCR output of Photos-3-001) ---
    # screen Notepad → "No.20"
    assert extract_pc_no_donate("ee  รร.วัดบ้านกล้วย\nNo.20\n7CY0DK2") == "20"
    # sticker "16" comes out clean on its own line
    img16 = "14117240630\nSERVICE TAG(S/N): 6HH1GL2\nEXPRESS SERVICE COD\n16\n10101"
    assert extract_pc_no_donate(img16) == "16", extract_pc_no_donate(img16)
    # sticker "4" fused with Thai-as-latin garbage "nnu4" → still recovers "4"
    img4 = "13969387766\nSERVICE TAG(S/N): 6F10GL2\nEXPRESS SERVICE CODE\nnnu4\n1010I\nWCH"
    assert extract_pc_no_donate(img4) == "4", extract_pc_no_donate(img4)
    #   ...and must NOT pick "10" from the Service Tag 6F10GL2, nor the Express code
    assert extract_pc_no_donate(img4) not in {"10", "139", "136"}
    # single digit "1" dropped by PaddleOCR entirely → empty (best-effort, human fills in Review)
    img1 = "14219509286\nSERVICE TAG(S/N): 6J5XFL2\nEXPRESS SERVICE CODE\n1010I\nO"
    assert extract_pc_no_donate(img1) == "", extract_pc_no_donate(img1)

    # --- position-aware: sticker (left) wins over express/tag/IO (center-right) ---
    # mirrors real RapidOCR boxes: sticker "Hainn2"(=หมู่วิทยา 2) far left; noise center-right
    res = [
        (_box(900), "14219509286", 1.0),                  # express — far right
        (_box(500), "SERVICE TAG(S/N): 6HWWFL2", 0.94),   # service tag — center
        (_box(560), "10101", 0.84),                        # IO noise — center-right
        (_box(190), "Hainn2", 0.64),                       # sticker — LEFT → "2"
    ]
    assert sticker_no_from_boxes(res) == "2", sticker_no_from_boxes(res)
    # must NOT pick express/IO/tag digits from the right
    assert sticker_no_from_boxes(res) not in {"10", "14", "21"}
    # left side has no digit (true detector-skip) → empty
    res_noleft = [(_box(900), "14219509286", 1.0), (_box(110), "O", 0.6)]
    assert sticker_no_from_boxes(res_noleft) == ""
    assert sticker_no_from_boxes([]) == ""

    # --- _trailing_sticker_no: เลขท้ายบรรทัดที่มีอักษรไทย (จาก crop OCR) ---
    assert _trailing_sticker_no("โรงเรียนวัดบ้านทราย 1") == "1"
    assert _trailing_sticker_no("หมู่วิทยา 2") == "2"
    assert _trailing_sticker_no("โรงเรียนบ้านหมู่วิทยา 16") == "16"
    # เลขใน Service Tag คนละบรรทัด (ไม่มีไทย) → ไม่หยิบ; เอาเลขจากบรรทัดไทย
    assert _trailing_sticker_no("โรงเรียนบ้าน\nSERVICE TAG(S/N): 6F10GL2\nหมู่วิทยา 4") == "4"
    # เลขไทย ๕ (barcode noise) ต้องไม่ match (ASCII only) → บรรทัดไทยไม่มีเลข ASCII, ไม่มี cand อื่น
    assert _trailing_sticker_no("โรงเรียน ๕๔๓") == ""
    # ไม่มีบรรทัดไทยเลย → fallback หยิบเลข ASCII ทั้งข้อความ (best-effort)
    assert _trailing_sticker_no("no thai here 7") == "7"

    # --- fuse_sticker_no: model หล่นหลัก → ใช้ crop; crop เพี้ยน → ใช้ model ---
    assert fuse_sticker_no("2", "20") == "20"      # model หล่น '0'
    assert fuse_sticker_no("8", "29") == "8"        # crop hallucinate → model
    assert fuse_sticker_no("22", "15") == "22"      # ไม่ subseq → model
    assert fuse_sticker_no("7", "") == "7"          # crop ว่าง
    assert fuse_sticker_no("", "20") == "20"        # model ว่าง

    # --- extract_no_donate_explicit: DonateMore (typed/printed) มาก่อน fusion ---
    # screen Notepad "NO.7" / "no.20" / "laptop no.63"
    scr7 = "LAPTOP\nSerialNumber\nNO.7\n792CXZ2\n"
    assert extract_no_donate_explicit(scr7) == "7", extract_no_donate_explicit(scr7)
    assert extract_no_donate_explicit("desktop\nno.20\n6GZWFL2") == "20"
    # chassis "Laptop Donate" → เลขหลังคำ Donate (กัน stray '256' ที่อยู่ก่อนหน้า)
    chassis = "256\nLaptop Donate\nSERVICE TAG(S/N):FORPWZ2\n32698293806\n11\n"
    assert extract_no_donate_explicit(chassis) == "11", extract_no_donate_explicit(chassis)
    # range hint กรองเลขนอกช่วง ('SN PC 1-990')
    assert extract_no_donate_explicit("New PC Donate\n677\n", "1-990") == "677"
    # Photos-3-001 (สติกเกอร์ไทย ไม่มี No./Donate) → '' → fusion คุมต่อ (ไม่ regress)
    assert extract_no_donate_explicit("โรงเรียนลพบุรีปัญญานุกูล 20\nSERVICE TAG(S/N): 6BS1GL2") == ""

    # --- extract_serial_donate: ผ่อน alpha filter (Dell tag 2-อักษร) แต่ตัด express code ---
    assert extract_serial_donate("SerialNumber\nB6W7103") == "B6W7103"   # 2 alpha (เดิม ≥3 ตก)
    assert extract_serial_donate("SERVICE TAG(S/N):6L74GL2") == "6L74GL2"
    assert extract_serial_donate("EXPRESS SERVICE CODE\n14342449142") == ""  # ตัวเลขล้วน → ไม่ใช่ serial

    print("donate parser self-check: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
