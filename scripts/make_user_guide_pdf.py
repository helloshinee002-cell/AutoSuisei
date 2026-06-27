#!/usr/bin/env python3
"""
Generate AutoSuisei User Guide PDF.

Uses fpdf2 with Tahoma (system font) for Thai support.

Output: docs/AutoSuisei_User_Guide.pdf
"""
from pathlib import Path

from fpdf import FPDF


ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
SCREENSHOTS = DOCS / "screenshots"
OUTPUT = DOCS / "AutoSuisei_User_Guide.pdf"

# Theme tokens (mirror Claude Design)
ACCENT = (16, 185, 129)   # #10B981 emerald
TEXT_DIM = (107, 123, 120)  # #6B7B78
BG_SURFACE = (245, 247, 246)  # light surface for printed PDF
DARK_TEXT = (15, 20, 22)  # near-black for body text

FONT_REG = r"C:\Windows\Fonts\tahoma.ttf"
FONT_BOLD = r"C:\Windows\Fonts\tahomabd.ttf"


class GuidePDF(FPDF):
    def __init__(self):
        super().__init__(orientation="P", unit="mm", format="A4")
        self.set_auto_page_break(auto=True, margin=18)
        self.set_margins(left=18, top=18, right=18)
        self.add_font("Tahoma", "", FONT_REG)
        self.add_font("Tahoma", "B", FONT_BOLD)
        self.alias_nb_pages()

    def header(self):
        if self.page_no() == 1:
            return  # cover page handles its own header
        self.set_font("Tahoma", "B", 9)
        self.set_text_color(*TEXT_DIM)
        self.cell(0, 8, "AutoSuisei — User Guide", new_x="LMARGIN", new_y="NEXT",
                   align="L")
        self.set_draw_color(*ACCENT)
        self.set_line_width(0.6)
        y = self.get_y()
        self.line(18, y, 192, y)
        self.ln(4)

    def footer(self):
        self.set_y(-12)
        self.set_font("Tahoma", "", 8)
        self.set_text_color(*TEXT_DIM)
        self.cell(0, 6, f"AutoSuisei  •  หน้า {self.page_no()} / {{nb}}",
                   align="C")

    # --- helpers ---------------------------------------------------------

    def _reset_x(self):
        """Force cursor back to left margin before multi_cell to avoid
        'not enough horizontal space' errors from prior cell/image calls."""
        self.set_x(self.l_margin)

    def h1(self, text: str):
        self._reset_x()
        self.set_font("Tahoma", "B", 18)
        self.set_text_color(*DARK_TEXT)
        self.cell(0, 10, text, new_x="LMARGIN", new_y="NEXT")
        self.set_draw_color(*ACCENT)
        self.set_line_width(0.8)
        y = self.get_y()
        self.line(18, y, 60, y)
        self.ln(5)

    def h2(self, text: str):
        self._reset_x()
        self.ln(2)
        self.set_font("Tahoma", "B", 13)
        self.set_text_color(*DARK_TEXT)
        self.cell(0, 8, text, new_x="LMARGIN", new_y="NEXT")
        self.ln(1)

    def body(self, text: str):
        self._reset_x()
        self.set_font("Tahoma", "", 11)
        self.set_text_color(*DARK_TEXT)
        self.multi_cell(0, 6, text)
        self.ln(1)

    def bullet(self, text: str):
        self._reset_x()
        self.set_font("Tahoma", "", 11)
        self.set_text_color(*DARK_TEXT)
        self.set_x(self.l_margin + 5)
        self.multi_cell(0, 6, "• " + text)

    def dim_caption(self, text: str):
        self._reset_x()
        self.set_font("Tahoma", "", 9)
        self.set_text_color(*TEXT_DIM)
        self.multi_cell(0, 5, text)
        self.set_text_color(*DARK_TEXT)
        self.ln(1)

    def image_with_caption(self, path: Path, caption: str, max_w: float = 174.0):
        if not path.exists():
            self.body(f"[ภาพไม่พบ: {path.name}]")
            return
        # Draw image centered
        x = (210 - max_w) / 2.0
        self.image(str(path), x=x, w=max_w)
        self.ln(2)
        self._reset_x()
        self.dim_caption(caption)


def build():
    pdf = GuidePDF()

    # ============ COVER PAGE ============
    pdf.add_page()
    # accent block top
    pdf.set_fill_color(*ACCENT)
    pdf.rect(0, 0, 210, 4, "F")

    pdf.ln(60)
    pdf.set_font("Tahoma", "B", 36)
    pdf.set_text_color(*ACCENT)
    pdf.cell(0, 16, "AutoSuisei", align="C", new_x="LMARGIN", new_y="NEXT")
    pdf.ln(2)
    pdf.set_font("Tahoma", "", 14)
    pdf.set_text_color(*TEXT_DIM)
    pdf.cell(0, 8, "Inventory OCR — PaddleOCR-powered desktop tool",
              align="C", new_x="LMARGIN", new_y="NEXT")
    pdf.cell(0, 6, "ดึง No. + Serial จากภาพถ่าย → ตรวจ/แก้ → rename อัตโนมัติ",
              align="C", new_x="LMARGIN", new_y="NEXT")

    pdf.ln(40)
    pdf.set_font("Tahoma", "", 12)
    pdf.set_text_color(*DARK_TEXT)
    pdf.cell(0, 7, "User Guide  •  Version 1.0.4", align="C",
              new_x="LMARGIN", new_y="NEXT")
    pdf.set_font("Tahoma", "", 10)
    pdf.set_text_color(*TEXT_DIM)
    pdf.cell(0, 6, "https://github.com/helloshinee002-cell/AutoSuisei",
              align="C")

    # ============ OVERVIEW ============
    pdf.add_page()
    pdf.h1("ภาพรวม")
    pdf.body(
        "AutoSuisei เป็น Windows desktop app ที่ใช้ PaddleOCR "
        "(rapidocr-onnxruntime) อ่านเลข No. (เลขกำกับบนสติกเกอร์/กระดาษ) "
        "และ Serial Number จากภาพถ่ายอุปกรณ์ IT (PC, Laptop, Monitor, "
        "Accessory) แล้วช่วยให้ผู้ใช้ตรวจสอบ แก้ไข และเปลี่ยนชื่อไฟล์ภาพ "
        "เป็นข้อมูลที่อ่านได้ในไม่กี่คลิก"
    )

    pdf.h2("ฟีเจอร์หลัก")
    pdf.bullet("Bulk extract เป็นชุด — 3 categories: PC&Laptop / Monitor / Accessory")
    pdf.bullet("Folder Watch — auto-process ไฟล์ใหม่ที่มาเข้าโฟลเดอร์")
    pdf.bullet("Review tab — ดูภาพ + แก้ค่า + verify + เปลี่ยนชื่อไฟล์")
    pdf.bullet("Rotation fallback — รองรับภาพที่ถ่ายตะแคง/หมุน (90°/180°/270°)")
    pdf.bullet("Barcode-first Serial (PC + Donate) — อ่าน Dell barcode ก่อน (Data Matrix สำหรับ laptop, "
               "Code128 สำหรับ desktop); แม่นกว่า OCR แก้ปัญหาอ่านสับสน O↔0; อ่านไม่ออกใช้ OCR + คอลัมน์ Src")
    pdf.bullet("Self-contained installer — ฝัง Python + rapidocr + onnxruntime")

    pdf.h2("Requirements")
    pdf.bullet("Windows 10/11 64-bit")
    pdf.bullet("RAM 4 GB ขึ้นไป (PaddleOCR ใช้ ~1.5 GB)")
    pdf.bullet("Disk ~500 MB สำหรับติดตั้ง (รวม embedded Python)")

    pdf.h2("การติดตั้ง")
    pdf.body(
        "1. ดาวน์โหลด AutoSuisei-Setup-1.0.4.exe จาก GitHub Releases\n"
        "2. ดับเบิ้ลคลิกไฟล์ติดตั้ง (อาจต้องสิทธิ์ admin)\n"
        "3. เปิดโปรแกรมจากเมนู Start → AutoSuisei"
    )

    # ============ OCR SINGLE TAB ============
    pdf.add_page()
    pdf.h1("แท็บ 1 — OCR Single Image")
    pdf.dim_caption(
        "Bulk extract ภาพในโฟลเดอร์ — เลือก category เพื่อใช้ parser ที่เหมาะสม"
    )
    pdf.image_with_caption(
        SCREENSHOTS / "ocr_single.png",
        "หน้าจอ OCR Single Image — ตารางแสดงผลการอ่าน 4 ภาพ "
        "(# / File / No. / Serial / Batch / Date) สถานะด้านล่างแสดง "
        "“Found 18 images. Loading PaddleOCR Model loaded in 0.5s. Processing”"
    )

    pdf.h2("ขั้นตอนใช้งาน")
    pdf.bullet(
        "กด PC&Laptop / Monitor / Accessory ปุ่มใดปุ่มหนึ่ง — "
        "เลือก category ตามชนิดอุปกรณ์ในภาพ"
    )
    pdf.bullet(
        "Dialog จะเปิดให้เลือกโฟลเดอร์ภาพ (Monitor และ Accessory มี default path)"
    )
    pdf.bullet(
        "โปรแกรมจะโหลด PaddleOCR (~1 วินาที) แล้วประมวลผลภาพละ ~0.5–1 วินาที — "
        "ดูความคืบหน้าที่ status bar ด้านล่าง"
    )
    pdf.bullet(
        "ถ้าต้องการยกเลิก กด Stop ระหว่างประมวลผล — ผลที่ทำไปแล้ว "
        "ยังอยู่ในตารางครบ ใช้ Send to Review ส่งต่อได้"
    )
    pdf.bullet(
        "กด Send to Review เพื่อเปิดผลในแท็บ Review (เพื่อตรวจ/แก้/rename)"
    )

    pdf.h2("Parser per category")
    pdf.bullet("PC&Laptop — Dell Service Tag 7 ตัวอักษร (เช่น HKFF5D2)")
    pdf.bullet("Monitor — Dell S/N ตัวเต็ม CN-XXXXX-XXXXX-XXX-XXXX-A00")
    pdf.bullet(
        "Accessory — flexible: หา label “S/N:” → numeric+dashes → "
        "8–15 digit line, ข้าม CBA asset barcode"
    )

    # ============ FOLDER WATCH TAB ============
    pdf.add_page()
    pdf.h1("แท็บ 2 — Folder Watch")
    pdf.dim_caption(
        "Auto-process ไฟล์ใหม่ที่ไหลเข้าโฟลเดอร์เฝ้าดู — เหมาะสำหรับ scan-as-you-go"
    )
    pdf.image_with_caption(
        SCREENSHOTS / "folder_watch.png",
        "หน้าจอ Folder Watch — กำลังเฝ้า C:/Users/hello/Downloads/Test2 "
        "(LIVE) พร้อม KPI cards: Total / Today / Avg conf. / Pending rev."
    )

    pdf.h2("ขั้นตอนใช้งาน")
    pdf.bullet("กด Choose folder… เลือกโฟลเดอร์ที่ต้องการเฝ้าดูไฟล์ใหม่")
    pdf.bullet("กด Start watching — สถานะเปลี่ยนเป็น “● LIVE” สีเขียว")
    pdf.bullet(
        "เมื่อมีภาพใหม่ลงโฟลเดอร์ โปรแกรมจะตรวจจับ (debounce 700ms) "
        "แล้วประมวลผลทันที — KPI cards อัปเดต real-time"
    )
    pdf.bullet("กด Stop watching เพื่อหยุดเฝ้า (worker Python จะปิด)")
    pdf.bullet("Send to Review ส่งผลทั้งหมดไปแท็บ Review สำหรับตรวจ/rename")

    pdf.h2("KPI cards คือ")
    pdf.bullet("Total — จำนวนภาพที่ประมวลผลทั้งหมดในเซสชันนี้")
    pdf.bullet("Today — เฉพาะภาพที่ photo_date ตรงกับวันนี้ (จากชื่อไฟล์)")
    pdf.bullet("Avg conf. — ค่าเฉลี่ย confidence ของ OCR ทุกภาพ")
    pdf.bullet("Pending rev. — ภาพที่ขาด No. หรือ Serial (ต้องตรวจมือ)")

    # ============ REVIEW TAB ============
    pdf.add_page()
    pdf.h1("แท็บ 3 — Review and Rename")
    pdf.dim_caption(
        "ตรวจสอบผล OCR + แก้ค่าด้วยมือ + Verify + เปลี่ยนชื่อไฟล์ภาพ"
    )
    pdf.image_with_caption(
        SCREENSHOTS / "review_rename.png",
        "หน้าจอ Review — ตารางผล 4 ภาพ + image preview ใหญ่ทางขวา (รูปจริง) "
        "+ form แก้ค่า (No., Serial, Batch, Date, Notes) + Apply + Next "
        "+ Rename by checkbox (No./Serial/Notes)"
    )

    pdf.h2("ขั้นตอนใช้งาน")
    pdf.bullet(
        "ถ้ามาจากแท็บ OCR/Watch (กด Send to Review) — ตารางจะถูกโหลดอัตโนมัติ"
    )
    pdf.bullet(
        "ถ้าโหลดจาก CSV เปล่า — กด Load CSV… แล้วเลือกไฟล์ csv "
        "(header ต้องมี filename column) จากนั้นกด Images folder… "
        "เลือกโฟลเดอร์ที่มีไฟล์ภาพจริง"
    )
    pdf.bullet(
        "คลิกแถวในตาราง → ภาพ preview ขึ้นทางขวา + form ทางขวาเติมค่าจาก OCR"
    )
    pdf.bullet(
        "แก้ค่าใน No. และ Serial ได้ ส่วน Batch/Date เป็น read-only (ข้อมูล OCR)"
    )
    pdf.bullet(
        "ติ๊ก Verified แล้วกด Apply + Next unverified — ระบบจะกระโดดไป "
        "แถวต่อไปที่ยังไม่ verified อัตโนมัติ"
    )
    pdf.bullet("กด Save ground truth เพื่อ export CSV ผลที่ verified แล้ว")

    pdf.h2("Rename ไฟล์")
    pdf.body(
        "ติ๊กฟิลด์ที่ต้องการใช้ในชื่อไฟล์ใหม่ (No. / Serial / Notes) — "
        "ระบบจะเชื่อมด้วย “_” แล้วเปลี่ยนชื่อไฟล์ภาพในโฟลเดอร์ทั้งหมด"
    )
    pdf.bullet("ตัวอย่าง: ติ๊ก No.+Serial → 15_6GSWFL2.jpg")
    pdf.bullet("ถ้าชื่อซ้ำกัน ระบบจะเติม -2, -3 ท้ายอัตโนมัติ")
    pdf.bullet(
        "อักขระต้องห้ามใน Windows filename (\\ / : * ? \" < > |) จะถูกแทนด้วย “_”"
    )

    # ============ TIPS / FAQ ============
    pdf.add_page()
    pdf.h1("เคล็ดลับ & FAQ")

    pdf.h2("ภาพถ่ายแบบไหนได้ผลดี")
    pdf.bullet("ภาพคมชัด ไม่เบลอ — แสงเพียงพอ")
    pdf.bullet(
        "เลขเขียนด้วยปากกาดำหนา บนกระดาษขาว — OCR อ่านได้ดีกว่าลายมือบาง"
    )
    pdf.bullet(
        "Dell Service Tag/S/N ในรูปต้องเห็นชัด ไม่ถูกบัง — "
        "ภาพถ่ายเฉียงหรือสะท้อนแสงจะลด accuracy"
    )
    pdf.bullet(
        "Rotation fallback ทำงานอัตโนมัติ — ภาพถ่ายตะแคง (90°/180°/270°) "
        "ก็ยังอ่านได้ (ช้ากว่าภาพปกติ ~2 เท่า)"
    )

    pdf.h2("ถ้า No. หรือ Serial หาไม่เจอ")
    pdf.bullet("กลับมาที่แท็บ Review แล้วคลิกแถวที่ขาดค่า — กรอกด้วยมือ")
    pdf.bullet(
        "ดู image preview เพื่ออ่านเอง — ถ้าไม่ชัดอาจต้องถ่ายภาพใหม่"
    )
    pdf.bullet(
        "Notes field ใช้ใส่หมายเหตุได้ เช่น “เลขเลือนรางมาก”, “สลากชำรุด”, "
        "“ของแถม” ฯลฯ — Notes จะติดไปกับชื่อไฟล์ใหม่ตอน rename ด้วย"
    )

    pdf.h2("ผลที่ทดสอบ")
    pdf.bullet(
        "Train PC&Laptop (632 ภาพ “Laptop 301-400”): No. 98.3% / Serial 85.1%"
    )
    pdf.bullet(
        "Train Monitor (754 ภาพ Dell monitor): No. 98.8% / Serial 94.7%"
    )
    pdf.bullet(
        "Train Monitor 2 (300 ภาพ มีภาพหมุน): No. 96.0% / Serial 96.3%"
    )
    pdf.bullet(
        "Train Accessory (418 ภาพ Olivetti/Verifone/Feitian): "
        "No. 66.3% / Serial 83.3%"
    )

    pdf.h2("ติดต่อ / แจ้งบั๊ก")
    pdf.body("เปิด Issue ที่ GitHub:")
    pdf.set_font("Tahoma", "", 11)
    pdf.set_text_color(*ACCENT)
    pdf.cell(0, 6,
              "https://github.com/helloshinee002-cell/AutoSuisei/issues",
              new_x="LMARGIN", new_y="NEXT")
    pdf.set_text_color(*DARK_TEXT)

    # ============ SAVE ============
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    pdf.output(str(OUTPUT))
    print(f"PDF: {OUTPUT}  ({OUTPUT.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    build()
