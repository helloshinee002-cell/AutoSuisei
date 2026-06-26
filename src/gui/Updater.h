#pragma once

#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
QT_END_NAMESPACE

namespace autopilot::gui {

/**
 * Check-version + auto-update ผ่าน **GitHub Releases REST API** (public repo, ไม่ต้องใช้ token).
 *
 * flow: `checkLatest()` → GET releases/latest → เทียบ `tag_name` กับ `currentVersion()`
 * (semver, [[VersionCompare.h]]) → emit `updateAvailable` / `upToDate` / `checkFailed`.
 * ถ้า user กด update → `downloadAndApply()` ดาวน์โหลด asset `update-package.zip` (partial patch;
 * ไม่มี → fallback full installer `*.exe`) → เขียน `apply-update.bat` (extract + overwrite + relaunch,
 * elevate ผ่าน UAC เพราะ Program Files) แล้ว quit ตัวเอง.
 */
class Updater : public QObject {
    Q_OBJECT
public:
    explicit Updater(QObject* parent = nullptr);

    /** เวอร์ชันปัจจุบันของ app (จาก compile-def `APP_VERSION`, เช่น "1.0.0"). */
    static QString currentVersion();

    /** owner ของ repo บน GitHub (สำหรับ API URL). */
    static QString repoSlug();

public slots:
    void checkLatest();       ///< async; emit หนึ่งใน upToDate/updateAvailable/checkFailed
    void downloadAndApply();  ///< เรียกหลัง updateAvailable — ดาวน์โหลด + apply + quit

signals:
    void upToDate(const QString& current);
    void updateAvailable(const QString& latestTag, const QString& notes);
    void checkFailed(const QString& message);
    void downloadProgress(qint64 received, qint64 total);
    void applyFailed(const QString& message);

private:
    void onCheckFinished(QNetworkReply* reply);
    void startDownload(const QString& url, bool isInstaller);
    bool launchApplyAndQuit(const QString& downloadedPath, bool isInstaller);

    QNetworkAccessManager* nam_;
    QString pkgUrl_;            ///< browser_download_url ของ update-package.zip (ถ้ามี)
    QString installerUrl_;      ///< fallback: full installer .exe asset
};

}  // namespace autopilot::gui
