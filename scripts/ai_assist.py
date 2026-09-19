#!/usr/bin/env python3
"""
AI assist (v1.0.5) — ส่งภาพให้ vision LLM ผ่าน LiteLLM (OpenAI-compatible) อ่าน No. + Serial
แล้ว merge กับผล OCR/บาร์โค้ด. ลำดับความเชื่อ: barcode > AI > OCR.

Config (env — GUI ใส่ให้จากหน้า AI Settings; ไม่มี key = ปิด AI, pipeline เดิม 100%):
    AUTOSUISEI_AI_KEY    LiteLLM virtual key (ห้าม hardcode / commit)
    AUTOSUISEI_AI_URL    default https://litellm.mashirotk.site  (https เท่านั้น ยกเว้น localhost)
    AUTOSUISEI_AI_MODEL  default gemma4:31b-cloud

ความปลอดภัย: key ส่งไปที่ URL ที่ตั้งไว้เท่านั้น — ไม่ follow redirect, ไม่รับ http:// (ยกเว้น loopback),
ไม่ log key. ความทน: error ใด ๆ → None (ใช้ผล OCR), fail ติดกัน 3 ครั้ง → พัก AI 5 นาที (ไม่ให้ทุกภาพรอ timeout).

stdlib only (urllib) → ไม่เพิ่ม pip dep (update-package ยังใช้ได้).

Self-check (ไม่เรียกเน็ต):
    python scripts/ai_assist.py --selfcheck
"""
import base64
import json
import os
import re
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

DEFAULT_URL = "https://litellm.mashirotk.site"
DEFAULT_MODEL = "gemma4:31b-cloud"
TIMEOUT_SEC = 60
MAX_SIDE = 1600  # ย่อภาพก่อนส่ง — ตัวอักษรบนป้ายยังอ่านออก, payload เล็ก
BREAKER_FAILS = 3        # fail ติดกันกี่ครั้งถึงพัก AI
BREAKER_COOLDOWN = 300   # วินาที
_LOOPBACK = {"localhost", "127.0.0.1", "::1"}
USER_AGENT = "AutoSuisei/1.0.5 (ai_assist)"

_HINTS = {
    "pc": "Serial = Dell Service Tag: exactly 7 uppercase letters/digits (never contains O or I), "
          "printed after 'SERVICE TAG' or 'S/N'. Ignore bank asset tags starting with 'CBA'.",
    "donate": "Serial = Dell Service Tag (7 chars, never O or I) or the value after 'SerialNumber' "
              "in a wmic screen. No. = the number written/typed on the sticker or Notepad ('NO.7', "
              "'Donate 677').",
    "monitor": "Serial = full Dell monitor S/N like CN-0JF27G-FCC00-76M-AKNB-A00 (starts with CN-). "
               "Never return the 7-char Service Tag. No. = the number on the white paper sticker.",
    "accessory": "Serial = the device S/N (label 'S/N' or 'Serial', or the digits under its barcode). "
                 "Ignore bank asset tags starting with 'CBA'.",
}

_PROMPT = (
    "This photo shows an IT device ({category}) labelled for inventory. Read two values:\n"
    "- no: the inventory number (digits only) written or printed on a sticker/paper/screen, "
    "often after 'No.'. Empty string if none.\n"
    "- serial: the device serial. {hint}\n"
    "Copy characters exactly as printed; if unsure or not visible, use an empty string. "
    'Reply with JSON only: {{"no": "...", "serial": "..."}}'
)

_warned: set[str] = set()
_state = {"fails": 0, "paused_until": 0.0}


def _warn_once(tag: str, msg: str) -> None:
    if tag not in _warned:
        _warned.add(tag)
        print(f"ai_assist: {msg}", file=sys.stderr)


def url_allowed(url: str) -> bool:
    """https ทุก host, http เฉพาะ loopback (key + ภาพต้องไม่วิ่ง cleartext)."""
    p = urllib.parse.urlsplit(url)
    if p.scheme == "https" and p.hostname:
        return True
    return p.scheme == "http" and (p.hostname or "") in _LOOPBACK


def config() -> dict | None:
    """env → {"key","url","model"}; ไม่มี key / key หรือ URL ไม่ปลอดภัย → None (AI ปิด)."""
    key = os.environ.get("AUTOSUISEI_AI_KEY", "").strip()
    if not key:
        return None
    if re.search(r"[\s\x00-\x1f\x7f]", key):  # key ที่มีขึ้นบรรทัด/ช่องว่างข้างใน = วางผิด (และจะหลุดใน error)
        _warn_once("key", "AI disabled: key contains whitespace/line break (fix it in AI Settings)")
        return None
    url = (os.environ.get("AUTOSUISEI_AI_URL", "").strip() or DEFAULT_URL).rstrip("/")
    if not url_allowed(url):
        _warn_once("url", "AI disabled: URL must be https:// (http only for localhost)")
        return None
    return {"key": key, "url": url,
            "model": os.environ.get("AUTOSUISEI_AI_MODEL", "").strip() or DEFAULT_MODEL}


class _NoRedirect(urllib.request.HTTPRedirectHandler):
    """urllib ค่าเริ่มต้นส่ง Authorization ต่อไปยัง Location (host อื่น/ลดเป็น http ก็ได้) → ห้าม follow."""

    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


_OPENER = urllib.request.build_opener(_NoRedirect)


def _encode_image(img_path) -> str | None:
    """อ่าน (unicode path ได้) + ย่อ + JPEG base64. None ถ้าอ่านไม่ได้."""
    import cv2
    from bulk_extract import _imread_unicode
    im = _imread_unicode(img_path)
    if im is None:
        return None
    h, w = im.shape[:2]
    scale = MAX_SIDE / max(h, w)
    if scale < 1:
        im = cv2.resize(im, (max(1, int(w * scale)), max(1, int(h * scale))),
                        interpolation=cv2.INTER_AREA)
    ok, buf = cv2.imencode(".jpg", im, [cv2.IMWRITE_JPEG_QUALITY, 85])
    return base64.b64encode(buf.tobytes()).decode("ascii") if ok else None


def parse_reply(content) -> dict | None:
    """ดึง JSON object ตัวแรกจากคำตอบ (ทน ```json fence / ข้อความรอบ ๆ)."""
    if not isinstance(content, str):
        return None
    m = re.search(r"\{.*\}", content, re.S)
    if not m:
        return None
    try:
        obj = json.loads(m.group(0))
    except ValueError:
        return None
    if not isinstance(obj, dict):
        return None
    return {"no": str(obj.get("no") or "").strip(),
            "serial": str(obj.get("serial") or "").strip()}


def ai_read(img_path, category: str) -> dict | None:
    """ถาม LLM อ่าน No./Serial จากภาพ. ผิดพลาดใด ๆ → None (ห้ามทำ pipeline พัง)."""
    cfg = config()
    if cfg is None or time.monotonic() < _state["paused_until"]:
        return None
    try:
        b64 = _encode_image(img_path)
        if b64 is None:
            return None
        prompt = _PROMPT.format(category=category, hint=_HINTS.get(category, _HINTS["pc"]))
        body = {
            "model": cfg["model"],
            "temperature": 0,
            "messages": [{"role": "user", "content": [
                {"type": "text", "text": prompt},
                {"type": "image_url", "image_url": {"url": f"data:image/jpeg;base64,{b64}"}},
            ]}],
        }
        req = urllib.request.Request(
            cfg["url"] + "/v1/chat/completions",
            data=json.dumps(body).encode("utf-8"),
            # User-Agent: Cloudflare หน้า litellm.mashirotk.site บล็อก UA ค่าเริ่มต้น "Python-urllib/x.y"
            # (403 "error code: 1010" ทุกภาพ) — ใส่ UA ของแอปตรง ๆ (ไม่ปลอมเป็น browser)
            headers={"Content-Type": "application/json",
                     "Authorization": f"Bearer {cfg['key']}",
                     "User-Agent": USER_AGENT},
        )
        with _OPENER.open(req, timeout=TIMEOUT_SEC) as resp:
            data = json.loads(resp.read().decode("utf-8"))
        _state["fails"] = 0
        return parse_reply(data["choices"][0]["message"]["content"])
    except Exception as e:  # noqa: BLE001 — best-effort: http.client/cv2/json/อะไรก็ตาม → fallback OCR
        # ไม่ log key: ตัดทุกที่ที่ key อาจโผล่ใน message ออก
        print(f"ai_assist: {type(e).__name__}: {str(e).replace(cfg['key'], '***')[:200]}",
              file=sys.stderr)
        _state["fails"] += 1
        if _state["fails"] >= BREAKER_FAILS:
            _state["fails"] = 0
            _state["paused_until"] = time.monotonic() + BREAKER_COOLDOWN
            # stderr ข้อความ ASCII — console/Qt ของ embedded python ไม่ใช่ UTF-8 เสมอ
            print(f"ai_assist: {BREAKER_FAILS} failures in a row - AI paused {BREAKER_COOLDOWN}s "
                  "(using OCR meanwhile)", file=sys.stderr)
        return None


def _clean_serial(s: str, category: str) -> str:
    """normalize + validate serial จาก AI ตามหมวด; ไม่ผ่าน → ''."""
    from bulk_extract import MONITOR_SERIAL_RE, _reformat_cn, _valid_dell_tag
    s = re.sub(r"\s+", "", s or "").upper()
    if not s:
        return ""
    if category == "monitor":
        if "-" not in s:
            s = _reformat_cn(s)
        return s if MONITOR_SERIAL_RE.fullmatch(s) else ""
    if category in ("pc", "donate"):
        # ASCII guard: isalpha/isdigit ใน _valid_dell_tag รับอักษร/เลขไทยด้วย
        if not (re.fullmatch(r"[A-Z0-9]{7}", s) and _valid_dell_tag(s)):
            return ""
        # Dell Service Tag ไม่มี O/I — AI สับสน O↔0 ต้องไม่ทับค่า OCR (donate อาจเป็น wmic serial → ไม่บังคับ)
        return "" if category == "pc" and set(s) & {"O", "I"} else s
    # accessory: alnum+dash 5-20 + ต้องมีตัวเลข (กันคำตอบ placeholder เช่น "UNKNOWN"/"NOT VISIBLE") + ไม่ใช่ CBA
    ok = (re.fullmatch(r"[A-Z0-9][A-Z0-9\-]{3,18}[A-Z0-9]", s) and re.search(r"[0-9]", s)
          and not s.startswith("CBA"))
    return s if ok else ""


def _clean_no(n: str, range_hint: str) -> str:
    """เลขล้วน 1-4 หลัก ([0-9] ไม่ใช่ \\d), ไม่ใช่ 0 ล้วน, อยู่ใน range hint ถ้ามี; ไม่ผ่าน → ''.
    คง 0 นำหน้าไว้ (ป้ายจริงมีแบบ '023')."""
    from bulk_extract import parse_range_bounds
    n = re.sub(r"\s+", "", n or "")
    if not re.fullmatch(r"[0-9]{1,4}", n) or int(n) == 0:
        return ""
    lo, hi = parse_range_bounds(range_hint)
    if lo and hi and not lo <= int(n) <= hi:
        return ""
    return n


def merge(pc_no: str, serial: str, serial_source: str, ai: dict | None,
          category: str, range_hint: str = "") -> tuple[str, str, str, str]:
    """barcode > AI > OCR. คืน (pc_no, serial, serial_source, no_source)."""
    no_source = "ocr"
    if not ai:
        return pc_no, serial, serial_source, no_source
    if serial_source != "barcode":
        ai_serial = _clean_serial(ai.get("serial", ""), category)
        # AI อ่านได้แค่ส่วนต้นของ serial ที่ OCR อ่านครบ (เช่น CN ตัดท้าย) → คงค่า OCR ที่ยาวกว่า
        truncated = serial and len(serial) > len(ai_serial) and serial.startswith(ai_serial)
        if ai_serial and not truncated:
            serial, serial_source = ai_serial, "ai"
    ai_no = _clean_no(ai.get("no", ""), range_hint)
    if ai_no:
        pc_no, no_source = ai_no, "ai"
    return pc_no, serial, serial_source, no_source


def _selfcheck() -> int:
    # parse_reply: fence + ข้อความรอบ ๆ + content ที่ไม่ใช่ string
    assert parse_reply('```json\n{"no": "15", "serial": "6GSWFL2"}\n```') == \
        {"no": "15", "serial": "6GSWFL2"}
    assert parse_reply("no json here") is None
    assert parse_reply(None) is None and parse_reply([{"type": "text"}]) is None
    assert parse_reply('{"no": 7}') == {"no": "7", "serial": ""}
    # barcode ไม่ถูก AI ทับ; No. จาก AI
    assert merge("1", "4TVQZL2", "barcode", {"no": "15", "serial": "4TVQZ12"}, "pc") == \
        ("15", "4TVQZL2", "barcode", "ai")
    # OCR serial ถูก AI แทน (O↔0) — แต่ AI ที่ตอบ O/I ใน pc ต้องไม่ทับ OCR
    assert merge("", "4NOTZL2", "ocr", {"no": "", "serial": "4N0TZL2"}, "pc") == \
        ("", "4N0TZL2", "ai", "ocr")
    assert merge("", "4N0TZL2", "ocr", {"no": "", "serial": "4NOTZL2"}, "pc") == \
        ("", "4N0TZL2", "ocr", "ocr")
    # AI serial ไม่ผ่าน validator → คง OCR
    assert merge("3", "ABC1234", "ocr", {"no": "x", "serial": "NOPE"}, "pc") == \
        ("3", "ABC1234", "ocr", "ocr")
    # monitor: dashless CN → reformat; Service Tag ไม่รับ; CN ที่ AI อ่านขาดท้ายไม่ทับ OCR ที่ครบ
    m = merge("", "", "ocr", {"no": "", "serial": "CN0YKH877287273RFUCIA00"}, "monitor")
    assert m[1].startswith("CN-") and m[2] == "ai", m
    assert merge("", "", "ocr", {"no": "", "serial": "27W2NG2"}, "monitor")[2] == "ocr"
    full = "CN-0JF27G-FCC00-76M-AKNB-A00"
    assert merge("", full, "ocr", {"no": "", "serial": "CN-0JF27G-FCC00-76M"}, "monitor")[1:3] == \
        (full, "ocr")
    # accessory: placeholder ไม่มีตัวเลข ไม่ทับ OCR
    assert merge("12", "261-535-477", "ocr", {"no": "", "serial": "Not visible"}, "accessory")[1:3] == \
        ("261-535-477", "ocr")
    assert merge("", "", "ocr", {"no": "", "serial": "261-535-477"}, "accessory")[2] == "ai"
    # No.: 0 ล้วนไม่รับ, 0 นำหน้าคงไว้, range hint กรอง
    assert merge("12", "", "ocr", {"no": "0", "serial": ""}, "pc") == ("12", "", "ocr", "ocr")
    assert merge("", "", "ocr", {"no": "023", "serial": ""}, "pc")[0] == "023"
    assert merge("317", "", "ocr", {"no": "17", "serial": ""}, "pc", "301-400")[0] == "317"
    assert merge("", "", "ocr", {"no": "317", "serial": ""}, "pc", "301-400")[0] == "317"
    # เลข/อักษรไทยจาก AI ต้องถูกปัด ([0-9] ไม่ใช่ \d)
    assert merge("5", "ABC1234", "ocr", {"no": "๑๕", "serial": "๖GSWFL2"}, "pc") == \
        ("5", "ABC1234", "ocr", "ocr")
    # AI ปิด → ไม่เปลี่ยนอะไร
    assert merge("5", "X", "ocr", None, "pc") == ("5", "X", "ocr", "ocr")
    # URL: https ได้, http เฉพาะ loopback
    assert url_allowed("https://litellm.mashirotk.site")
    assert url_allowed("http://localhost:4000") and url_allowed("http://127.0.0.1")
    assert not url_allowed("http://litellm.mashirotk.site") and not url_allowed("ftp://x")
    # config: key ที่มีขึ้นบรรทัดข้างใน / URL http → ปิด AI
    saved = {k: os.environ.get(k) for k in ("AUTOSUISEI_AI_KEY", "AUTOSUISEI_AI_URL")}
    try:
        os.environ["AUTOSUISEI_AI_KEY"] = "sk-abc\nxyz"
        assert config() is None
        os.environ["AUTOSUISEI_AI_KEY"] = "sk-abc"
        os.environ["AUTOSUISEI_AI_URL"] = "http://evil.example"
        assert config() is None
        os.environ["AUTOSUISEI_AI_URL"] = "https://litellm.mashirotk.site/"
        assert config()["url"] == "https://litellm.mashirotk.site"
    finally:
        for k, v in saved.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v
    print("ai_assist selfcheck: OK")
    return 0


if __name__ == "__main__":
    if "--selfcheck" in sys.argv:
        sys.exit(_selfcheck())
    if len(sys.argv) >= 2:  # manual probe: ai_assist.py <image> [category]
        print(json.dumps(ai_read(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "pc")))
        sys.exit(0)
    print(__doc__)
