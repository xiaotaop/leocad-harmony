#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
#include "lc_authorizedsave.h"

class AuthorizedSaveTest : public QObject
{
    Q_OBJECT
    static QByteArray Read(const QString& Path)
    {
        QFile File(Path);
        if (!File.open(QIODevice::ReadOnly)) return {};
        return File.readAll();
    }
private slots:
    void newUnicodeDocument()
    {
        QTemporaryDir Dir;
        const QString Path = Dir.filePath(QString::fromUtf8("模型 roundtrip.ldr"));
        const QByteArray Bytes("0 Model\r\n1 4 -30 0 10 1 0 0 0 1 0 0 0 1 3003.dat\r\n");
        const auto Result = lcWriteAuthorizedDocument(Path, Bytes);
        QVERIFY2(Result.Success, qPrintable(Result.Error));
        QCOMPARE(Read(Path), Bytes);
    }
    void shorterOverwriteRemovesOldTail()
    {
        QTemporaryDir Dir;
        const QString Path = Dir.filePath("existing.mpd");
        QVERIFY(lcWriteAuthorizedDocument(Path, QByteArray(4096, 'x')).Success);
        QVERIFY(lcWriteAuthorizedDocument(Path, QByteArray("0 New\r\n")).Success);
        QCOMPARE(Read(Path), QByteArray("0 New\r\n"));
    }
    void failedPartialOverwriteRestoresOriginal()
    {
        QTemporaryDir Dir;
        const QString Path = Dir.filePath("existing.ldr");
        const QByteArray Original("0 Original document must survive\r\n");
        QVERIFY(lcWriteAuthorizedDocument(Path, Original).Success);
        const auto Result = lcWriteAuthorizedDocument(Path, QByteArray("0 Replacement\r\n"),
            [](QFile& File, const QByteArray& Data) { File.write(Data.constData(), 3); File.flush(); return false; });
        QVERIFY(!Result.Success);
        QVERIFY(!Result.Error.isEmpty());
        QCOMPARE(Read(Path), Original);
        QVERIFY(Result.RecoveryPath.isEmpty());
    }
    void failedNewWriteRemovesPartialFile()
    {
        QTemporaryDir Dir;
        const QString Path = Dir.filePath("new.ldr");
        const auto Result = lcWriteAuthorizedDocument(Path, QByteArray("0 New\r\n"),
            [](QFile& File, const QByteArray& Data) { File.write(Data.constData(), 2); return false; });
        QVERIFY(!Result.Success);
        QVERIFY(!QFile::exists(Path));
    }
    void invalidDestinationDoesNotPublish()
    {
        QTemporaryDir Dir;
        const auto Result = lcWriteAuthorizedDocument(Dir.path(), QByteArray("0 Model\r\n"));
        QVERIFY(!Result.Success);
        QVERIFY(!Result.Error.isEmpty());
        QVERIFY(QFileInfo(Dir.path()).isDir());
    }
    void mismatchedReadbackRestoresOriginal()
    {
        QTemporaryDir Dir;
        const QString Path = Dir.filePath("existing.ldr");
        const QByteArray Original("0 Original\r\n");
        QVERIFY(lcWriteAuthorizedDocument(Path, Original).Success);
        const auto Result = lcWriteAuthorizedDocument(Path, QByteArray("0 New\r\n"),
            [](QFile& File, const QByteArray&) { return lcWriteDocumentBytes(File, QByteArray("0 Wrong\r\n")); });
        QVERIFY(!Result.Success);
        QCOMPARE(Read(Path), Original);
    }
    void failedRollbackRetainsRecoveryCopy()
    {
        QTemporaryDir Dir;
        const QString Path = Dir.filePath("existing.ldr");
        const QByteArray Original("0 Original\r\n");
        QVERIFY(lcWriteAuthorizedDocument(Path, Original).Success);
        const auto Result = lcWriteAuthorizedDocument(Path, QByteArray("0 New\r\n"),
            [](QFile& File, const QByteArray&) { File.close(); return false; });
        QVERIFY(!Result.Success);
        QVERIFY(!Result.RecoveryPath.isEmpty());
        QCOMPARE(Read(Result.RecoveryPath), Original);
        QVERIFY(QFile::remove(Result.RecoveryPath));
    }
};
QTEST_GUILESS_MAIN(AuthorizedSaveTest)
#include "tst_authorized_save.moc"
