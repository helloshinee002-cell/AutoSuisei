#include "Updater.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <nlohmann/json.hpp>

#include "VersionCompare.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace autopilot::gui {

namespace {
constexpr char kApiLatest[] =
    "https://api.github.com/repos/helloshinee002-cell/AutoSuisei/releases/latest";
constexpr char kExeName[] = "AutoSuisei.exe";
}  // namespace

Updater::Updater(QObject* parent)
    : QObject(parent), nam_(new QNetworkAccessManager(this)) {}

QString Updater::currentVersion() {
#ifdef APP_VERSION
    return QStringLiteral(APP_VERSION);
#else
    return QStringLiteral("0.0.0");
#endif
}

QString Updater::repoSlug() { return QStringLiteral("helloshinee002-cell/AutoSuisei"); }

void Updater::checkLatest() {
    QNetworkRequest req((QUrl(QString::fromLatin1(kApiLatest))));
    // public repo → ไม่ต้องใส่ token (rate-limit 60/hr/IP พอสำหรับ check). User-Agent บังคับโดย GitHub.
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent", "AutoSuisei-Updater");
    req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onCheckFinished(reply); });
}

void Updater::onCheckFinished(QNetworkReply* reply) {
    reply->deleteLater();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        if (status == 403 && reply->rawHeader("X-RateLimit-Remaining") == "0") {
            emit checkFailed(tr("GitHub API ติด rate limit — ลองใหม่ภายหลัง"));
        } else if (status == 404) {
            emit checkFailed(tr("ยังไม่มี release บน GitHub (404)"));
        } else {
            emit checkFailed(tr("เชื่อมต่อ GitHub ไม่ได้: %1").arg(reply->errorString()));
        }
        return;
    }

    const QByteArray body = reply->readAll();
    try {
        const auto j = nlohmann::json::parse(body.toStdString());
        const QString tag = QString::fromStdString(j.value("tag_name", std::string{}));
        const QString notes = QString::fromStdString(j.value("body", std::string{}));
        pkgUrl_.clear();
        installerUrl_.clear();
        for (const auto& a : j.value("assets", nlohmann::json::array())) {
            const QString name = QString::fromStdString(a.value("name", std::string{}));
            const QString url =
                QString::fromStdString(a.value("browser_download_url", std::string{}));
            if (name.compare("update-package.zip", Qt::CaseInsensitive) == 0) {
                pkgUrl_ = url;
            } else if (name.endsWith(".exe", Qt::CaseInsensitive)) {
                installerUrl_ = url;
            }
        }
        if (tag.isEmpty()) {
            emit checkFailed(tr("อ่าน tag_name จาก release ไม่ได้"));
            return;
        }
        if (isVersionNewer(tag.toStdString(), currentVersion().toStdString())) {
            emit updateAvailable(tag, notes);
        } else {
            emit upToDate(currentVersion());
        }
    } catch (const std::exception& e) {
        emit checkFailed(tr("parse JSON ของ release ล้มเหลว: %1").arg(e.what()));
    }
}

void Updater::downloadAndApply() {
    // partial patch ก่อน (เล็ก/เร็ว); ไม่มี → full installer
    if (!pkgUrl_.isEmpty()) {
        startDownload(pkgUrl_, /*isInstaller=*/false);
    } else if (!installerUrl_.isEmpty()) {
        startDownload(installerUrl_, /*isInstaller=*/true);
    } else {
        emit applyFailed(tr("release นี้ไม่มีไฟล์ update-package.zip หรือ installer"));
    }
}

void Updater::startDownload(const QString& url, bool isInstaller) {
    QNetworkRequest req((QUrl(url)));
    req.setRawHeader("User-Agent", "AutoSuisei-Updater");
    // browser_download_url redirect ไป CDN — Qt6 ตาม redirect ให้ default
    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, &Updater::downloadProgress);
    connect(reply, &QNetworkReply::finished, this, [this, reply, isInstaller]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit applyFailed(tr("ดาวน์โหลดล้มเหลว: %1").arg(reply->errorString()));
            return;
        }
        const QString dest = QDir(QDir::tempPath())
                                 .filePath(isInstaller ? "AutoSuisei-Setup-update.exe"
                                                       : "autosuisei-update-package.zip");
        QFile f(dest);
        if (!f.open(QIODevice::WriteOnly) || f.write(reply->readAll()) < 0) {
            emit applyFailed(tr("เขียนไฟล์ที่ดาวน์โหลดไม่ได้: %1").arg(dest));
            return;
        }
        f.close();
        if (!launchApplyAndQuit(dest, isInstaller)) {
            emit applyFailed(tr("เริ่มกระบวนการติดตั้งอัปเดตไม่ได้"));
        }
    });
}

bool Updater::launchApplyAndQuit(const QString& downloadedPath, bool isInstaller) {
#ifdef _WIN32
    const QString installDir = QCoreApplication::applicationDirPath();

    if (isInstaller) {
        // Inno Setup silent — ตัว installer จัดการปิด/แทนที่เอง
        const std::wstring exe = QDir::toNativeSeparators(downloadedPath).toStdWString();
        const std::wstring params = L"/SILENT /NORESTART";
        auto rc = reinterpret_cast<INT_PTR>(
            ShellExecuteW(nullptr, L"runas", exe.c_str(), params.c_str(), nullptr, SW_SHOWNORMAL));
        if (rc <= 32) return false;
        QCoreApplication::quit();
        return true;
    }

    // partial: เขียน .bat ที่ รอ exe ปิด → extract zip → xcopy ทับ → relaunch
    const QString extractDir = QDir(QDir::tempPath()).filePath("autosuisei-update-extract");
    const QString batPath = QDir(QDir::tempPath()).filePath("autosuisei-apply-update.bat");
    const QString nZip = QDir::toNativeSeparators(downloadedPath);
    const QString nDest = QDir::toNativeSeparators(installDir);
    const QString nExtract = QDir::toNativeSeparators(extractDir);

    QString bat;
    bat += "@echo off\r\n";
    bat += "setlocal\r\n";
    bat += ":waitloop\r\n";
    bat += "tasklist /FI \"IMAGENAME eq " + QString(kExeName) +
           "\" 2>nul | find /I \"" + QString(kExeName) + "\" >nul && (timeout /t 1 /nobreak >nul & goto waitloop)\r\n";
    bat += "rmdir /S /Q \"" + nExtract + "\" >nul 2>nul\r\n";
    bat += "powershell -NoProfile -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath '" +
           nZip + "' -DestinationPath '" + nExtract + "' -Force\"\r\n";
    bat += "xcopy \"" + nExtract + "\\*\" \"" + nDest + "\\\" /E /Y /I >nul\r\n";
    bat += "rmdir /S /Q \"" + nExtract + "\" >nul 2>nul\r\n";
    bat += "del \"" + nZip + "\" >nul 2>nul\r\n";
    bat += "start \"\" \"" + nDest + "\\" + QString(kExeName) + "\"\r\n";
    bat += "(goto) 2>nul & del \"%~f0\"\r\n";

    QFile bf(batPath);
    if (!bf.open(QIODevice::WriteOnly) || bf.write(bat.toUtf8()) < 0) return false;
    bf.close();

    // Program Files = protected → ต้อง elevate (UAC) ผ่าน verb "runas"
    const std::wstring batW = QDir::toNativeSeparators(batPath).toStdWString();
    auto rc = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"runas", L"cmd.exe",
                      (L"/c \"" + batW + L"\"").c_str(), nullptr, SW_HIDE));
    if (rc <= 32) return false;  // ผู้ใช้กด No บน UAC หรือ error
    QCoreApplication::quit();
    return true;
#else
    Q_UNUSED(downloadedPath);
    Q_UNUSED(isInstaller);
    emit applyFailed(tr("auto-update รองรับเฉพาะ Windows"));
    return false;
#endif
}

}  // namespace autopilot::gui
