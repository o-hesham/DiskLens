#include "../src/core/analysis/DuplicateFinder.h"

#include <QtTest/QtTest>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <filesystem>

class DuplicateFinderTests final : public QObject
{
    Q_OBJECT

private slots:
    void groupsOnlyRepeatedSizes()
    {
        // Synthetic metadata fixtures; no filesystem access.
        DiskLens::FileEntry first;
        first.absolutePath = QStringLiteral("first.txt");
        first.sizeBytes = 100;

        DiskLens::FileEntry second;
        second.absolutePath = QStringLiteral("second.txt");
        second.sizeBytes = 100;

        DiskLens::FileEntry unique;
        unique.absolutePath = QStringLiteral("unique.txt");
        unique.sizeBytes = 200;

        const auto groups = DiskLens::DuplicateFinder::findSizeCandidates({first, second, unique});

        QCOMPARE(groups.size(), qsizetype{1});
        QCOMPARE(groups.first().sizeBytes, qint64{100});
        QCOMPARE(groups.first().files.size(), qsizetype{2});
    }

    void ignoresIneligibleEntries()
    {
        // Synthetic metadata fixtures; no filesystem access.
        DiskLens::FileEntry file;
        file.sizeBytes = 100;

        DiskLens::FileEntry directory;
        directory.sizeBytes = 100;
        directory.isDirectory = true;

        DiskLens::FileEntry symlink;
        symlink.sizeBytes = 100;
        symlink.isSymbolicLink = true;

        DiskLens::FileEntry empty;

        const auto groups =
            DiskLens::DuplicateFinder::findSizeCandidates(
                {file, directory, symlink, empty, empty});

        QVERIFY(groups.isEmpty());
    }

    void identicalFilesFormOneGroup()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QByteArray contents("identical contents");
        QVector<DiskLens::FileEntry> entries;

        for (const auto &name :
             {QStringLiteral("first.bin"), QStringLiteral("second.bin")})
        {
            QFile file(directory.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(contents), qint64(contents.size()));
            QVERIFY(file.flush());
            file.close();

            DiskLens::FileEntry entry;
            entry.absolutePath = file.fileName();
            entry.name = name;
            entry.sizeBytes = contents.size();
            entry.lastModified = QFileInfo(file).lastModified();
            entries.append(entry);
        }

        QVector<DiskLens::DuplicateProgress> updates;
        DiskLens::CancellationToken token;

        const auto result = DiskLens::DuplicateFinder::findDuplicates(
            entries, token,
            [&](const DiskLens::DuplicateProgress &progress)
            {
                updates.append(progress);
            });

        QCOMPARE(result.status, DiskLens::DuplicateStatus::Completed);
        QCOMPARE(result.groups.size(), qsizetype{1});

        const auto &group = result.groups.first();
        QCOMPARE(group.files.size(), qsizetype{2});
        QCOMPARE(group.sizeBytes, qint64(contents.size()));
        QCOMPARE(group.digest, QCryptographicHash::hash(contents, QCryptographicHash::Sha256));
        QCOMPARE(result.ignoredHardLinkAliases, qsizetype{0});
        QVERIFY(result.issues.isEmpty());
        QVERIFY(result.errorMessage.isEmpty());

        const QVector<qint64> expectedCounts{0, 0, 1, 1, 2};

        for (auto phase : {DiskLens::DuplicatePhase::SampleHashing,
                           DiskLens::DuplicatePhase::FullHashing})
        {
            QVector<DiskLens::DuplicateProgress> phaseUpdates;
            for (const auto &update : updates)
                if (update.phase == phase)
                    phaseUpdates.append(update);

            QCOMPARE(phaseUpdates.size(), expectedCounts.size());

            for (qsizetype i = 0; i < phaseUpdates.size(); ++i)
            {
                QCOMPARE(phaseUpdates.at(i).totalOperations, qint64{2});
                QCOMPARE(phaseUpdates.at(i).completedOperations, expectedCounts.at(i));

                const QString expectedPath = i == 0 ? QString{} : entries.at((i - 1) / 2).absolutePath;

                QCOMPARE(phaseUpdates.at(i).currentPath, expectedPath);
            }
        }
    }

    void collapsesHardLinkAliases()
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

        QVector<DiskLens::FileEntry> entries;

        for (const auto &path : {originalPath, aliasPath})
        {
            const QFileInfo info(path);
            DiskLens::FileEntry entry;
            entry.absolutePath = path;
            entry.sizeBytes = info.size();
            entry.lastModified = info.lastModified();
            entries.append(entry);
        }

        DiskLens::CancellationToken token;
        const auto result =
            DiskLens::DuplicateFinder::findDuplicates(entries, token);

        QCOMPARE(result.status, DiskLens::DuplicateStatus::Completed);
        QVERIFY(result.groups.isEmpty());
        QCOMPARE(result.ignoredHardLinkAliases, qsizetype{1});
        QVERIFY(result.issues.isEmpty());
        QVERIFY(result.errorMessage.isEmpty());

        const QString copyPath = directory.filePath("copy.bin");
        QVERIFY(QFile::copy(originalPath, copyPath));

        const QFileInfo copyInfo(copyPath);
        DiskLens::FileEntry copy;
        copy.absolutePath = copyPath;
        copy.sizeBytes = copyInfo.size();
        copy.lastModified = copyInfo.lastModified();
        entries.append(copy);

        const auto withCopy =
            DiskLens::DuplicateFinder::findDuplicates(entries, token);

        QCOMPARE(withCopy.status, DiskLens::DuplicateStatus::Completed);
        QCOMPARE(withCopy.groups.size(), qsizetype{1});

        const auto &group = withCopy.groups.first();
        QCOMPARE(group.files.size(), qsizetype{2});
        QCOMPARE(group.sizeBytes, qint64{4});
        QCOMPARE(group.ignoredHardLinkAliases, qsizetype{1});
        QCOMPARE(withCopy.ignoredHardLinkAliases, qsizetype{1});
        QVERIFY(withCopy.issues.isEmpty());
        QVERIFY(withCopy.errorMessage.isEmpty());
    }

    void respectsCancellationBeforeAnalysis()
    {
        DiskLens::CancellationToken token;
        token.requestCancellation();

        const auto result =
            DiskLens::DuplicateFinder::findDuplicates({}, token);

        QCOMPARE(result.status, DiskLens::DuplicateStatus::Cancelled);
        QVERIFY(result.groups.isEmpty());
        QVERIFY(result.issues.isEmpty());
        QCOMPARE(result.ignoredHardLinkAliases, qsizetype{0});
        QVERIFY(result.errorMessage.isEmpty());
    }

    void matchingSamplesDoNotProveDuplicates()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        QVector<DiskLens::FileEntry> entries;

        for (const auto &name :
             {QStringLiteral("first.bin"), QStringLiteral("second.bin")})
        {
            QByteArray contents(3 * 64 * 1024, 'A');

            // Change only a middle byte in the second file.
            if (entries.size() == 1)
                contents[64 * 1024] = 'B';

            QFile file(directory.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(contents), qint64(contents.size()));
            QVERIFY(file.flush());
            file.close();

            const QFileInfo info(file);
            DiskLens::FileEntry entry;
            entry.absolutePath = file.fileName();
            entry.sizeBytes = info.size();
            entry.lastModified = info.lastModified();
            entries.append(entry);
        }

        DiskLens::CancellationToken token;
        const auto result =
            DiskLens::DuplicateFinder::findDuplicates(entries, token);

        QCOMPARE(result.status, DiskLens::DuplicateStatus::Completed);
        QVERIFY(result.groups.isEmpty());
        QVERIFY(result.issues.isEmpty());
        QCOMPARE(result.ignoredHardLinkAliases, qsizetype{0});
        QVERIFY(result.errorMessage.isEmpty());
    }

    void skipsMissingFileAndFindsRemainingCopies()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QByteArray contents("identical contents");
        QVector<DiskLens::FileEntry> entries;

        for (const auto &name :
             {QStringLiteral("first.bin"), QStringLiteral("second.bin"), QStringLiteral("third.bin")})
        {
            QFile file(directory.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(contents), qint64(contents.size()));
            QVERIFY(file.flush());
            file.close();

            DiskLens::FileEntry entry;
            entry.absolutePath = file.fileName();
            entry.name = name;
            entry.sizeBytes = contents.size();
            entry.lastModified = QFileInfo(file).lastModified();
            entries.append(entry);
        }

        // Remove one temporary file after recording its scan metadata.
        const QString missingPath = entries.first().absolutePath;
        QVERIFY(QFile::remove(missingPath));

        DiskLens::CancellationToken token;
        const auto result = DiskLens::DuplicateFinder::findDuplicates(entries, token);

        QCOMPARE(result.status, DiskLens::DuplicateStatus::Completed);
        QCOMPARE(result.groups.size(), qsizetype{1});

        const auto &group = result.groups.first();
        QCOMPARE(group.files.size(), qsizetype{2});
        QCOMPARE(group.sizeBytes, qint64(contents.size()));
        QCOMPARE(group.digest, QCryptographicHash::hash(contents, QCryptographicHash::Sha256));
        QCOMPARE(result.ignoredHardLinkAliases, qsizetype{0});
        QCOMPARE(result.issues.size(), qsizetype{1});
        QCOMPARE(result.issues.first().absolutePath, missingPath);
        QVERIFY(!result.issues.first().message.isEmpty());

        for (const auto &entry : group.files)
            QVERIFY(entry.absolutePath != missingPath);

        QCOMPARE(result.issues.first().kind, DiskLens::DuplicateIssueKind::Missing);

        QVERIFY(result.errorMessage.isEmpty());
    }

    void reportsSizeGroupingProgress()
    {
        // Synthetic directory metadata; no filesystem access needed.
        QVector<DiskLens::FileEntry> entries(2);
        for (auto &entry : entries)
            entry.isDirectory = true;

        QVector<DiskLens::DuplicateProgress> updates;
        DiskLens::CancellationToken token;

        const auto result = DiskLens::DuplicateFinder::findDuplicates(
            entries, token,
            [&](const DiskLens::DuplicateProgress &progress)
            {
                if (progress.phase == DiskLens::DuplicatePhase::SizeGrouping)
                    updates.append(progress);
            });

        QCOMPARE(result.status, DiskLens::DuplicateStatus::Completed);
        QVERIFY(result.groups.isEmpty());
        QVERIFY(result.issues.isEmpty());
        QCOMPARE(updates.size(), qsizetype{2});

        QCOMPARE(updates.first().completedOperations, qint64{0});
        QCOMPARE(updates.last().completedOperations, qint64{2});

        for (const auto &update : updates)
        {
            QCOMPARE(update.totalOperations, qint64{2});
            QVERIFY(update.currentPath.isEmpty());
        }
    }

    void respectsCancellationFromProgressCallback()
    {
        for (int cancelAt : {1, 2})
        {
            DiskLens::CancellationToken token;
            int callbackCount = 0;

            const auto result = DiskLens::DuplicateFinder::findDuplicates(
                {}, token,
                [&](const DiskLens::DuplicateProgress &)
                {
                    ++callbackCount;
                    if (callbackCount == cancelAt)
                        token.requestCancellation();
                });

            QCOMPARE(result.status, DiskLens::DuplicateStatus::Cancelled);
            QCOMPARE(callbackCount, cancelAt);
            QVERIFY(result.groups.isEmpty());
            QVERIFY(result.issues.isEmpty());
            QVERIFY(result.errorMessage.isEmpty());
        }
    }
};

QTEST_GUILESS_MAIN(DuplicateFinderTests)

#include "DuplicateFinderTests.moc"
