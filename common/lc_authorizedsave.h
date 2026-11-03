#pragma once

#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <functional>

struct lcAuthorizedSaveResult
{
    bool Success = false;
    QString Error;
    QString RecoveryPath;
};

inline bool lcWriteDocumentBytes(QFile& File, const QByteArray& Data)
{
    qint64 Offset = 0;
    while (Offset < Data.size())
    {
        const qint64 Written = File.write(Data.constData() + Offset, Data.size() - Offset);
        if (Written <= 0) return false;
        Offset += Written;
    }
    return File.resize(Data.size()) && File.flush();
}

// A document-picker grant permits the selected file, not a sibling temporary
// file. Use QFile's platform file engine for that grant. Call only after the
// complete model has been serialized; sandbox saves continue to use QSaveFile.
// This is a verified export with rollback, not an atomic rename. A local backup
// survives if rollback itself fails; abrupt process/power loss is not atomic.
inline lcAuthorizedSaveResult lcWriteAuthorizedDocument(const QString& Path, const QByteArray& Data,
    const std::function<bool(QFile&, const QByteArray&)>& Write = lcWriteDocumentBytes)
{
    const auto Fail = [](const QString& Error, const QString& Recovery = QString())
    { return lcAuthorizedSaveResult{false, Error.isEmpty() ? QCoreApplication::translate("Project", "File write failed") : Error, Recovery}; };

    if (QFileInfo(Path).isDir())
        return Fail(QCoreApplication::translate("Project", "Destination is a directory"));

    const bool Existed = QFileInfo::exists(Path);
    QByteArray Original;
    QTemporaryFile Backup(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
        .filePath(QStringLiteral("leocad-save-recovery-XXXXXX")));
    if (Existed)
    {
        QFile Previous(Path);
        if (!Previous.open(QIODevice::ReadOnly)) return Fail(Previous.errorString());
        Original = Previous.readAll();
        if (Previous.error() != QFileDevice::NoError) return Fail(Previous.errorString());
        Previous.close();
        const QString Cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (Cache.isEmpty() || !QDir().mkpath(Cache) || !Backup.open())
            return Fail(QCoreApplication::translate("Project", "Cannot create local recovery backup"));
        if (Backup.write(Original) != Original.size() || !Backup.flush())
            return Fail(QCoreApplication::translate("Project", "Cannot write local recovery backup"));
    }

    QFile Target(Path);
    // Do not truncate until the new bytes have been written. ReadWrite also
    // allows verification through the same authorized platform file engine.
    if (!Target.open(QIODevice::ReadWrite)) return Fail(Target.errorString());
    const bool Written = Write(Target, Data);
    const bool Verified = Written && Target.seek(0) && Target.readAll() == Data
        && Target.error() == QFileDevice::NoError;
    if (Verified)
    {
        Target.close();
        return {true, {}, {}};
    }

    const QString Error = Target.error() == QFileDevice::NoError
        ? QCoreApplication::translate("Project", "Saved file did not match the model.") : Target.errorString();
    if (Existed)
    {
        // A transient write failure must not silently destroy the previous
        // document. If storage remains unavailable, retain the private backup.
        const bool Restored = Target.seek(0) && lcWriteDocumentBytes(Target, Original)
            && Target.seek(0) && Target.readAll() == Original;
        Target.close();
        if (!Restored)
        {
            Backup.setAutoRemove(false);
            return Fail(Error, Backup.fileName());
        }
    }
    else
    {
        Target.close();
        if (!Target.remove()) return Fail(QCoreApplication::translate("Project", "Write failed; incomplete target could not be removed: %1").arg(Path));
    }
    return Fail(Error);
}
