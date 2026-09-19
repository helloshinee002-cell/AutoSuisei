---
tags: [idea, autosuisei, ai]
updated: 2026-09-18
---

# Idea — AI ช่วย OCR (vision LLM ผ่าน LiteLLM) → v1.0.5

[[index]] · [[CLAUDE]] · log: [[logs/2026-09-18-v1.0.5-ai-assist]]

## Problem
OCR ตันที่บางเคส: Monitor No. สติกเกอร์ไทย ~42%, donate เขียนมือ ~78%, Accessory No. 66%, Serial OCR สับสน O↔0
([[Accuracy-Results]]). free/local ไปต่อยาก ([[Dev-History]] trade-off triangle)

## ทำไม
ผู้ใช้มี LiteLLM ของตัวเอง (`https://litellm.mashirotk.site`) serve `gemma4:31b-cloud` → ใช้ vision LLM ได้โดยไม่ต้อง
bundle โมเดลใหญ่ (ยัง distributable: แอปแค่ส่ง HTTP)

## ขอบเขต (เคาะกับ user 2026-09-18)
- key กรอกในแอป (AI Settings) เก็บต่อเครื่อง — **ห้ามอยู่ในรีโป/installer**
- เรียก AI **ทุกภาพ** (OCR tab + Watch)
- ลำดับความเชื่อ **barcode > AI > OCR**; แสดงที่มาในคอลัมน์ Src ("AI", "No.:AI")
- ไม่มี key/ปิด AI → พฤติกรรม v1.0.4 เดิม

## Acceptance
- ไม่ regress เมื่อ AI ปิด / server ล่ม (fallback OCR เงียบ ๆ)
- ผู้ใช้ทดสอบกับ key จริงบนภาพจริงแต่ละหมวด → ผ่านแล้วค่อย release v1.0.5 (update-package ใช้ได้ — ไม่เพิ่ม pip dep)
