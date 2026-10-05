#include "../src/core/filesystem/FileIdentity.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <QFileInfo>
#include <filesystem>

class FileIdentityTests final : public QObject
{
    Q_OBJECT

private slots:
    void identicalContentsAreSeparateFiles()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString firstPath = directory.filePath("first.bin");
        const QString secondPath = directory.filePath("second.bin");
        const QByteArray contents("identical contents");

        for (const auto &path : {firstPath, secondPath})
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(contents), qint64(contents.size()));
            QVERIFY(file.flush());
            file.close();
        }

        std::error_code error;
        const bool sameFile = DiskLens::referToSameFile(firstPath, secondPath, error);

        QVERIFY(!error);
        QVERIFY(!sameFile);
    }

    void hardLinksReferToSameFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString originalPath = directory.filePath("original.bin");
        const QString aliasPath = directory.filePath("alias.bin");

        QFile file(originalPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(QByteArray("data")), qint64{4});
        QVERIFY(file.flush());
        file.close();

        std::error_code error;
        std::filesystem::create_hard_link(
            QFileInfo(originalPath).filesystemFilePath(),
            QFileInfo(aliasPath).filesystemFilePath(),
            error);

        if (error == std::errc::operation_not_supported ||
            error == std::errc::function_not_supported)
            QSKIP("Hard links are unsupported on this test filesystem");

        QVERIFY2(!error, error.message().c_str());

        const bool sameFile =
            DiskLens::referToSameFile(originalPath, aliasPath, error);

        QVERIFY2(!error, error.message().c_str());
        QVERIFY(sameFile);
    }

    void missingFileReportsError()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString existingPath = directory.filePath("existing.bin");
        const QString missingPath = directory.filePath("missing.bin");

        QFile file(existingPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(QByteArray("data")), qint64{4});
        QVERIFY(file.flush());
        file.close();

        std::error_code error;
        const bool sameFile =
            DiskLens::referToSameFile(existingPath, missingPath, error);

        QVERIFY(!sameFile);
        QVERIFY(error);

        error.clear();

        const bool reversed =
            DiskLens::referToSameFile(missingPath, existingPath, error);

        QVERIFY(!reversed);
        QVERIFY(error);
    }
};

QTEST_GUILESS_MAIN(FileIdentityTests)

#include "FileIdentityTests.moc"