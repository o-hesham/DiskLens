#include "../src/core/analysis/FileHasher.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class FileHasherTests final : public QObject
{
    Q_OBJECT

private slots:
    void hashesSmallFileOnce()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QByteArray contents("sample content");
        QFile file(directory.filePath("sample.txt"));

        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(contents), qint64(contents.size()));
        QVERIFY(file.flush());
        file.close();

        DiskLens::FileEntry entry;
        entry.absolutePath = file.fileName();
        entry.sizeBytes = contents.size();
        entry.lastModified = QFileInfo(file).lastModified();

        DiskLens::CancellationToken token;
        const auto result = DiskLens::FileHasher::hashSample(entry, token);

        QCOMPARE(result.status, DiskLens::FileHashStatus::Completed);
        QCOMPARE(result.digest, QCryptographicHash::hash(contents, QCryptographicHash::Sha256));
        QVERIFY(result.errorMessage.isEmpty());
    }

    void hashesFirstAndLastChunks()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        constexpr qsizetype chunkSize = 64 * 1024;

        const QByteArray contents = QByteArray(chunkSize, 'A') + QByteArray(chunkSize, 'B') + QByteArray(chunkSize, 'C');

        QFile file(directory.filePath("large.bin"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(contents), qint64(contents.size()));
        QVERIFY(file.flush());
        file.close();

        DiskLens::FileEntry entry;
        entry.absolutePath = file.fileName();
        entry.sizeBytes = contents.size();
        entry.lastModified = QFileInfo(file).lastModified();

        DiskLens::CancellationToken token;
        const auto result = DiskLens::FileHasher::hashSample(entry, token);

        const QByteArray sample = contents.left(chunkSize) + contents.right(chunkSize);

        QCOMPARE(result.status, DiskLens::FileHashStatus::Completed);
        QCOMPARE(result.digest, QCryptographicHash::hash(sample, QCryptographicHash::Sha256));
    }

    void hashesEntireFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        constexpr qsizetype chunkSize = 64 * 1024;

        const QByteArray contents = QByteArray(chunkSize, 'A') + QByteArray(chunkSize, 'B') + QByteArray(chunkSize, 'C') + QByteArray(17, 'D');

        QFile file(directory.filePath("large.bin"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(contents), qint64(contents.size()));
        QVERIFY(file.flush());
        file.close();

        DiskLens::FileEntry entry;
        entry.absolutePath = file.fileName();
        entry.sizeBytes = contents.size();
        entry.lastModified = QFileInfo(file).lastModified();

        DiskLens::CancellationToken token;
        const auto result = DiskLens::FileHasher::hashFull(entry, token);

        QCOMPARE(result.status, DiskLens::FileHashStatus::Completed);
        QCOMPARE(result.digest, QCryptographicHash::hash(contents, QCryptographicHash::Sha256));
    }

    void respectsCancellationBeforeOpening()
    {
        DiskLens::FileEntry entry;

        DiskLens::CancellationToken token;
        token.requestCancellation();

        const auto result = DiskLens::FileHasher::hashSample(entry, token);
        QCOMPARE(result.status, DiskLens::FileHashStatus::Cancelled);
        QVERIFY(result.digest.isEmpty());
        QVERIFY(result.errorMessage.isEmpty());

        const auto fullResult = DiskLens::FileHasher::hashFull(entry, token);
        QCOMPARE(fullResult.status, DiskLens::FileHashStatus::Cancelled);
        QVERIFY(fullResult.digest.isEmpty());
        QVERIFY(fullResult.errorMessage.isEmpty());
    }

    void rejectsFileChangedSinceScan()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QByteArray original("original");
        QFile file(directory.filePath("changed.txt"));

        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(original), qint64(original.size()));
        QVERIFY(file.flush());
        file.close();

        // Capture the metadata that a scan would record.
        DiskLens::FileEntry entry;
        entry.absolutePath = file.fileName();
        entry.sizeBytes = original.size();
        entry.lastModified = QFileInfo(file).lastModified();

        // Change the file after that snapshot.
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Append));
        QCOMPARE(file.write(QByteArray("extra")), qint64{5});
        QVERIFY(file.flush());
        file.close();

        DiskLens::CancellationToken token;

        const auto result = DiskLens::FileHasher::hashSample(entry, token);
        QCOMPARE(result.status, DiskLens::FileHashStatus::Failed);
        QCOMPARE(result.issueKind, DiskLens::DuplicateIssueKind::Changed);
        QVERIFY(result.digest.isEmpty());
        QVERIFY(!result.errorMessage.isEmpty());

        const auto fullResult = DiskLens::FileHasher::hashFull(entry, token);
        QCOMPARE(fullResult.status, DiskLens::FileHashStatus::Failed);
        QCOMPARE(fullResult.issueKind, DiskLens::DuplicateIssueKind::Changed);
        QVERIFY(fullResult.digest.isEmpty());
        QVERIFY(!fullResult.errorMessage.isEmpty());
    }
};

QTEST_GUILESS_MAIN(FileHasherTests)

#include "FileHasherTests.moc"