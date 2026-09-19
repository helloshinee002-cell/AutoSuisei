#include "AiSettings.h"

#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QVariantMap>
#include <QVBoxLayout>

namespace autopilot::gui {

namespace {
// ค่า default ต้องตรงกับ scripts/ai_assist.py (DEFAULT_URL / DEFAULT_MODEL)
constexpr auto kDefaultUrl = "https://litellm.mashirotk.site";
constexpr auto kDefaultModel = "gemma4:31b-cloud";

QSettings settings() { return QSettings("MashiroTK", "AutoSuisei"); }

// ตรงกับ ai_assist.config(): ว่าง → default, ตัด '/' ท้าย
QString normalizeUrl(QString u) {
    u = u.trimmed();
    while (u.endsWith('/')) u.chop(1);
    return u.isEmpty() ? QString(kDefaultUrl) : u;
}
QString urlValue(const QSettings& s) { return normalizeUrl(s.value("ai/url").toString()); }

// ตรงกับ ai_assist.url_allowed(): https ทุก host, http เฉพาะ loopback — key + ภาพห้ามวิ่ง cleartext
bool urlAllowed(const QString& u) {
    const QUrl url(u);
    if (url.scheme() == "https" && !url.host().isEmpty()) return true;
    const auto h = url.host();
    return url.scheme() == "http" && (h == "localhost" || h == "127.0.0.1" || h == "::1");
}

// key ที่มีช่องว่าง/ขึ้นบรรทัดข้างใน = วางผิด (Python จะปิด AI + ห้ามให้หลุดใน error message)
bool keyValid(const QString& k) { return !k.contains(QRegularExpression(R"([\s\x00-\x1f\x7f])")); }

QString modelValue(const QSettings& s) {
    const auto m = s.value("ai/model").toString().trimmed();
    return m.isEmpty() ? QString(kDefaultModel) : m;
}
// request ที่ส่ง key: ไม่ follow redirect (กัน key ไปโผล่ host อื่น — ตรงกับ ai_assist.py)
QNetworkRequest keyedRequest(const QString& url, const QString& key, int timeoutMs) {
    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("Authorization", "Bearer " + key.toUtf8());
    req.setTransferTimeout(timeoutMs);
    return req;
}

// ภาพทดสอบ: เลข "47" ตัวหนาบนพื้นขาว → PNG base64 (model text-only อ่านไม่ได้ → ตอบ "47" ไม่ได้)
QString probeImageBase64() {
    QImage img(120, 60, QImage::Format_RGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    QFont f("Arial", 30, QFont::Bold);
    p.setFont(f);
    p.setPen(Qt::black);
    p.drawText(img.rect(), Qt::AlignCenter, "47");
    p.end();
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return QString::fromLatin1(png.toBase64());
}

// cache ผลทดสอบอ่านภาพ ต่อ (URL, model) — เปิด dialog ครั้งถัดไปไม่ต้องรอทดสอบใหม่
QString visionKey(const QString& base, const QString& id) { return base + "|" + id; }
}  // namespace

AiSettingsDialog::AiSettingsDialog(QWidget* parent)
    : QDialog(parent), nam_(new QNetworkAccessManager(this)) {
    setWindowTitle("AI Settings");
    setMinimumWidth(460);
    const auto s = settings();

    enabled_ = new QCheckBox("ใช้ AI ช่วยอ่าน No. / Serial ทุกภาพ (ลำดับความเชื่อ: Barcode > AI > OCR)");
    enabled_->setChecked(s.value("ai/enabled", false).toBool());
    url_ = new QLineEdit(urlValue(s));
    model_ = new QComboBox();
    model_->addItem(modelValue(s));  // ค่าที่บันทึกไว้ — แทนด้วยรายการจริงเมื่อโหลดสำเร็จ
    key_ = new QLineEdit(s.value("ai/key").toString());
    key_->setEchoMode(QLineEdit::Password);
    key_->setPlaceholderText("LiteLLM virtual key (sk-…)");
    result_ = new QLabel();
    result_->setWordWrap(true);
    result_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* form = new QFormLayout();
    form->addRow("LiteLLM URL", url_);
    form->addRow("Model", model_);
    form->addRow("Virtual key", key_);

    testBtn_ = new QPushButton("โหลดรายการ model");
    testBtn_->setToolTip("ดึงรายชื่อ model ที่ virtual key นี้ใช้ได้จาก LiteLLM (ทดสอบการเชื่อมต่อไปด้วย)");
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->addButton(testBtn_, QDialogButtonBox::ActionRole);

    auto* root = new QVBoxLayout(this);
    root->addWidget(enabled_);
    root->addLayout(form);
    root->addWidget(new QLabel(
        "<small>ภาพจะถูกส่งไปยัง server ข้างบนเพื่อประมวลผล (https เท่านั้น) · key เก็บเฉพาะเครื่องนี้<br>"
        "Folder Watch ที่กำลังทำงานจะใช้ค่าใหม่ทันที · OCR ที่กำลังรันอยู่ใช้ค่าเดิมจนจบรอบ</small>"));
    root->addWidget(result_);
    root->addWidget(buttons);

    testBtn_->setToolTip("ดึงรายชื่อ model ของ key นี้แล้วทดสอบใหม่ทุกตัวว่าอ่านภาพได้ไหม");
    connect(testBtn_, &QPushButton::clicked, this, [this]() { loadModels(true); });
    // แก้ URL/key เสร็จ → โหลดรายการใหม่ (key ต่างกัน = model ที่ใช้ได้ต่างกัน) — ใช้ผลทดสอบที่ cache ไว้
    connect(url_, &QLineEdit::editingFinished, this, [this]() { loadModels(false); });
    connect(key_, &QLineEdit::editingFinished, this, [this]() { loadModels(false); });
    if (!key_->text().trimmed().isEmpty()) loadModels(false);  // มี key อยู่แล้ว → โหลดทันทีตอนเปิด
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (save()) accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

bool AiSettingsDialog::save() {
    const auto url = normalizeUrl(url_->text());
    const auto key = key_->text().trimmed();
    if (!urlAllowed(url)) {
        QMessageBox::warning(this, "AI Settings",
                             "URL ต้องเป็น https:// (http ใช้ได้เฉพาะ localhost) — key และภาพห้ามส่งแบบไม่เข้ารหัส");
        return false;
    }
    if (!keyValid(key)) {
        QMessageBox::warning(this, "AI Settings", "Virtual key มีช่องว่าง/ขึ้นบรรทัดอยู่ข้างใน — วางใหม่อีกครั้ง");
        return false;
    }
    if (enabled_->isChecked() && model_->currentText().trimmed().isEmpty()) {
        QMessageBox::warning(this, "AI Settings", "ยังไม่ได้เลือก model — กด \"โหลดรายการ model\" ก่อน");
        return false;
    }
    auto s = settings();
    s.setValue("ai/enabled", enabled_->isChecked());
    s.setValue("ai/url", url);
    s.setValue("ai/model", model_->currentText().trimmed());
    s.setValue("ai/key", key);
    return true;
}

void AiSettingsDialog::loadModels(bool force) {
    const auto base = normalizeUrl(url_->text());
    const auto key = key_->text().trimmed();
    if (key.isEmpty()) {
        result_->setText("ใส่ virtual key แล้วกด \"โหลดรายการ model\"");
        return;
    }
    if (!urlAllowed(base)) {
        result_->setText("❌ URL ต้องเป็น https:// (http ได้เฉพาะ localhost) — ไม่ส่ง key");
        return;
    }
    if (!keyValid(key)) {
        result_->setText("❌ key มีช่องว่าง/ขึ้นบรรทัดอยู่ข้างใน");
        return;
    }
    const int round = ++probeRound_;  // รอบใหม่ — ผลของรอบเก่าที่ยังค้างอยู่จะถูกทิ้ง
    result_->setText("กำลังโหลดรายการ model...");
    testBtn_->setEnabled(false);
    if (pending_) pending_->abort();
    auto* reply = nam_->get(keyedRequest(base + "/v1/models", key, 15000));
    pending_ = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, base, round, force]() {
        reply->deleteLater();
        if (reply != pending_ || round != probeRound_) return;  // ถูกแทนด้วยรอบใหม่แล้ว
        if (reply->error() != QNetworkReply::NoError) {
            // โหลดไม่ได้ → คง model เดิมใน dropdown (ค่าที่บันทึกไว้ไม่หาย)
            testBtn_->setEnabled(true);
            result_->setText("❌ เชื่อมต่อไม่ได้: " + reply->errorString());
            return;
        }
        QStringList ids;
        for (const auto& m : QJsonDocument::fromJson(reply->readAll())["data"].toArray()) {
            const auto id = m.toObject()["id"].toString();
            if (!id.isEmpty()) ids << id;
        }
        if (ids.isEmpty()) {
            testBtn_->setEnabled(true);
            result_->setText("⚠️ เชื่อมต่อได้ แต่ key นี้ไม่มี model ที่ใช้ได้ — ตรวจใน LiteLLM");
            return;
        }
        probeOrder_ = ids;
        visionOk_.clear();
        visionNo_.clear();
        probeLeft_ = 0;
        const auto cache = settings().value("ai/visionCache").toMap();
        for (const auto& id : ids) {
            const auto k = visionKey(base, id);
            if (!force && cache.contains(k)) {
                (cache.value(k).toBool() ? visionOk_ : visionNo_) << id;
            } else {
                ++probeLeft_;
                probeModel(base, id, round);
            }
        }
        if (probeLeft_ == 0) finishProbe(ids.size());
        else result_->setText(QString("กำลังตรวจว่า model ไหนอ่านภาพได้ (เหลือ %1/%2)...")
                                  .arg(probeLeft_).arg(ids.size()));
    });
}

void AiSettingsDialog::probeModel(const QString& base, const QString& id, int round) {
    const QJsonObject body{
        {"model", id},
        {"temperature", 0},
        {"max_tokens", 10},
        {"messages", QJsonArray{QJsonObject{
                         {"role", "user"},
                         {"content", QJsonArray{
                              QJsonObject{{"type", "text"},
                                          {"text", "What number is in this image? Reply with digits only."}},
                              QJsonObject{{"type", "image_url"},
                                          {"image_url", QJsonObject{{"url", "data:image/png;base64," +
                                                                               probeImageBase64()}}}}}}}}}};
    auto req = keyedRequest(base + "/v1/chat/completions", key_->text().trimmed(), 25000);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto* reply = nam_->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, base, id, round]() {
        reply->deleteLater();
        if (round != probeRound_) return;  // รอบเก่า (แก้ key/URL ระหว่างทดสอบ)
        bool ok = false;
        if (reply->error() == QNetworkReply::NoError) {
            const auto doc = QJsonDocument::fromJson(reply->readAll());
            const auto text = doc["choices"][0]["message"]["content"].toString();
            ok = text.contains("47");
        }
        (ok ? visionOk_ : visionNo_) << id;
        auto s = settings();
        auto cache = s.value("ai/visionCache").toMap();
        cache.insert(visionKey(base, id), ok);
        s.setValue("ai/visionCache", cache);
        if (--probeLeft_ > 0) {
            result_->setText(QString("กำลังตรวจว่า model ไหนอ่านภาพได้ (เหลือ %1/%2)...")
                                 .arg(probeLeft_).arg(probeOrder_.size()));
            return;
        }
        finishProbe(probeOrder_.size());
    });
}

void AiSettingsDialog::finishProbe(int total) {
    testBtn_->setEnabled(true);
    QStringList usable;  // คงลำดับตาม LiteLLM
    for (const auto& id : probeOrder_)
        if (visionOk_.contains(id)) usable << id;
    QStringList hidden;
    for (const auto& id : probeOrder_)
        if (!visionOk_.contains(id)) hidden << id;
    const auto keep = model_->currentText();
    model_->clear();
    if (usable.isEmpty()) {
        result_->setText(QString("⚠️ key นี้มี %1 model แต่ไม่มีตัวไหนอ่านภาพได้ (%2)")
                             .arg(total).arg(hidden.join(", ")));
        return;
    }
    model_->addItems(usable);
    const int idx = usable.indexOf(keep);
    model_->setCurrentIndex(idx >= 0 ? idx : 0);
    auto msg = QString("✅ key นี้มี %1 model — อ่านภาพได้ %2").arg(total).arg(usable.size());
    if (!hidden.isEmpty())
        msg += QString(" (ซ่อน %1 ที่อ่านภาพไม่ได้: %2)").arg(hidden.size()).arg(hidden.join(", "));
    result_->setText(msg);
}

bool aiEnabled() {
    const auto s = settings();
    return s.value("ai/enabled", false).toBool() &&
           !s.value("ai/key").toString().trimmed().isEmpty();
}

QString aiModelName() { return modelValue(settings()); }

QProcessEnvironment aiProcessEnvironment() {
    auto env = QProcessEnvironment::systemEnvironment();
    env.remove("AUTOSUISEI_AI_KEY");  // ปิดใน GUI = ปิดจริง แม้ system env มีค่า
    if (!aiEnabled()) return env;
    const auto s = settings();
    env.insert("AUTOSUISEI_AI_KEY", s.value("ai/key").toString().trimmed());
    env.insert("AUTOSUISEI_AI_URL", urlValue(s));
    env.insert("AUTOSUISEI_AI_MODEL", modelValue(s));
    return env;
}

}  // namespace autopilot::gui
