#pragma once

#include <QDialog>
#include <QPointer>
#include <QProcessEnvironment>

class QCheckBox;
class QComboBox;
class QNetworkReply;
class QLabel;
class QLineEdit;
class QPushButton;
class QNetworkAccessManager;

namespace autopilot::gui {

/**
 * AI assist (v1.0.5) — ตั้งค่า vision LLM ผ่าน LiteLLM ที่ `scripts/ai_assist.py` ใช้.
 *
 * ค่าเก็บต่อผู้ใช้ด้วย QSettings (HKCU\Software\MashiroTK\AutoSuisei) — **ไม่อยู่ในรีโป/installer**
 * (repo public; ห้าม hardcode secrets). ส่งให้ Python ผ่าน env (ไม่ใช่ argv) ด้วย aiProcessEnvironment().
 * ponytail: key เก็บ plaintext ใน HKCU ของผู้ใช้ — ถ้าต้องเข้มขึ้นค่อยห่อ DPAPI.
 */
class AiSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit AiSettingsDialog(QWidget* parent = nullptr);

private:
    /** ตรวจ URL (https / http เฉพาะ localhost) + key (ไม่มีช่องว่างข้างใน) แล้วบันทึก; ไม่ผ่าน → false */
    bool save();
    /**
     * GET /v1/models ด้วย key ปัจจุบัน (เฉพาะ model ที่ key นี้ใช้ได้) → ทดสอบว่าอ่านภาพได้ไหม → dropdown แสดงเฉพาะตัวที่ผ่าน.
     * force=false ใช้ผลที่ cache ไว้ (ทดสอบเฉพาะ model ใหม่); force=true (ปุ่ม) ทดสอบใหม่ทุกตัว.
     */
    void loadModels(bool force);
    /** ส่งภาพเลข "47" ให้ model อ่าน — ผ่านเมื่อ HTTP 200 และคำตอบมี "47" (model text-only ตอบไม่ได้) */
    void probeModel(const QString& base, const QString& id, int round);
    /** ทดสอบครบทุกตัวแล้ว → เติม dropdown + สรุป */
    void finishProbe(int total);

    QCheckBox* enabled_{};
    QLineEdit* url_{};
    QComboBox* model_{};  ///< เลือกจากรายการที่ LiteLLM ให้ key นี้ (ไม่ให้พิมพ์เอง)
    QLineEdit* key_{};
    QLabel* result_{};
    QPushButton* testBtn_{};
    QNetworkAccessManager* nam_{};
    QPointer<QNetworkReply> pending_;  ///< request ล่าสุด — reply เก่าที่ช้ากว่าถูกทิ้ง
    int probeRound_{0};                ///< รอบทดสอบล่าสุด — ผลของรอบเก่า (แก้ key ระหว่างทดสอบ) ถูกทิ้ง
    int probeLeft_{0};
    QStringList probeOrder_;           ///< ลำดับ model ตาม LiteLLM
    QStringList visionOk_;
    QStringList visionNo_;
};

/** true ถ้าเปิดใช้ AI และมี key. */
[[nodiscard]] bool aiEnabled();

/** ชื่อ model ที่ตั้งไว้ (default gemma4:31b-cloud) — ใช้แสดงในป้ายโหมด. */
[[nodiscard]] QString aiModelName();

/** env ของ system + AUTOSUISEI_AI_{KEY,URL,MODEL} เมื่อเปิดใช้ (ปิดอยู่ = env เดิม, AI ไม่ทำงาน). */
[[nodiscard]] QProcessEnvironment aiProcessEnvironment();

}  // namespace autopilot::gui
