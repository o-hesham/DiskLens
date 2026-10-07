#include "../src/core/analysis/FileComparer.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QVector>
#include <QtTest/QtTest>

class FileComparerTests final : public QObject
{
    Q_OBJECT

private slots:
    // Qt calls this function to prepare the inputs for comparesFileContents().
    void comparesFileContents_data()
    {
        QTest::addColumn<int>("changedOffset");

        // -1 means leave both files unchanged.
        QTest::newRow("identical") << -1;
        // 0 means change the second file's first byte.
        QTest::newRow("different-first-byte") << 0;
        // The last byte's index is file size minus one.
        QTest::newRow("different-last-byte") << (64 * 1024 + 16);
    }

    // Qt runs this function once for each row defined above.
    void comparesFileContents()
    {
        // Get this run's value: -1, 0, or 65552.
        QFETCH(int, changedOffset);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QByteArray contents(64 * 1024 + 17, 'A');
        QVector<DiskLens::FileEntry> entries;

        for (const auto &name : {QStringLiteral("first.bin"), QStringLiteral("second.bin")})
        {
            QByteArray fileContents = contents;

            // First iteration: entries.size() is 0 → no change.
            // Second iteration: entries.size() is 1 → change this file
            // only if the current test row requests a changed byte.
            if (entries.size() == 1 && changedOffset >= 0)
                fileContents[changedOffset] = 'B';

            QFile file(directory.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(fileContents), qint64(contents.size()));
            QVERIFY(file.flush());
            file.close();

            DiskLens::FileEntry entry;
            entry.absolutePath = file.fileName();
            entry.sizeBytes = contents.size();
            entry.lastModified = QFileInfo(file).lastModified();
            entries.append(entry);
        }

        DiskLens::CancellationToken token;
        const auto result = DiskLens::FileComparer::compare(entries.at(0), entries.at(1), token);

        const auto expected = changedOffset < 0 ? DiskLens::FileComparisonStatus::Equal : DiskLens::FileComparisonStatus::Different;

        QCOMPARE(result.status, expected);
        QVERIFY(result.errorMessage.isEmpty());
    }

    void respectsCancellationBeforeOpening()
    {
        DiskLens::FileEntry first;
        DiskLens::FileEntry second;

        DiskLens::CancellationToken token;
        token.requestCancellation();

        const auto result = DiskLens::FileComparer::compare(first, second, token);

        QCOMPARE(result.status, DiskLens::FileComparisonStatus::Cancelled);
        QVERIFY(result.errorMessage.isEmpty());
    }

    void rejectsStaleModificationTime()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QByteArray contents("test");
        QFile file(directory.filePath("file.txt"));

        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(contents), qint64(contents.size()));
        QVERIFY(file.flush());
        file.close();

        DiskLens::FileEntry current;
        current.absolutePath = file.fileName();
        current.sizeBytes = contents.size();
        current.lastModified = QFileInfo(file).lastModified();
        QVERIFY(current.lastModified.isValid());

        // Test fixture: simulate an older scan timestamp.
        DiskLens::FileEntry stale = current;
        stale.lastModified = current.lastModified.addSecs(-10);

        QFile otherFile(directory.filePath("other.txt"));
        QVERIFY(otherFile.open(QIODevice::WriteOnly));
        QCOMPARE(otherFile.write(contents), qint64(contents.size()));
        QVERIFY(otherFile.flush());
        otherFile.close();

        DiskLens::FileEntry other = current;
        other.absolutePath = otherFile.fileName();
        other.lastModified = QFileInfo(otherFile).lastModified();
        QVERIFY(other.lastModified.isValid());

        DiskLens::CancellationToken token;

        const auto firstResult = DiskLens::FileComparer::compare(stale, other, token);

        QCOMPARE(firstResult.status, DiskLens::FileComparisonStatus::Failed);
        QVERIFY(!firstResult.errorMessage.isEmpty());

        const auto secondResult = DiskLens::FileComparer::compare(other, stale, token);

        QCOMPARE(secondResult.status, DiskLens::FileComparisonStatus::Failed);
        QVERIFY(!secondResult.errorMessage.isEmpty());

        QCOMPARE(firstResult.failedPath, stale.absolutePath);
        QCOMPARE(secondResult.failedPath, stale.absolutePath);
    }
};

QTEST_GUILESS_MAIN(FileComparerTests)

#include "FileComparerTests.moc"