#include "../src/core/analysis/DuplicateFinder.h"

#include <QtTest/QtTest>

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
};

QTEST_GUILESS_MAIN(DuplicateFinderTests)

#include "DuplicateFinderTests.moc"
