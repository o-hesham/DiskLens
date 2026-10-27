#include "../src/core/operations/OperationStore.h"
#include "../src/core/operations/RecoveryService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace
{
    DiskLens::FileEntry writeFile(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly))
            file.write(contents);
        file.close();

        const QFileInfo info(path);
        DiskLens::FileEntry entry;
        entry.absolutePath = info.absoluteFilePath();
        entry.name = info.fileName();
        entry.sizeBytes = info.size();
        entry.lastModified = info.lastModified();
        return entry;
    }

    QByteArray readFile(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }
} // namespace

class RecoveryServiceTests final : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir directory_;
    DiskLens::FileEntry kept_;
    DiskLens::FileEntry marked_;
    QVector<DiskLens::DuplicateGroup> groups_;

    QString recoveryRoot() const { return directory_.filePath(QStringLiteral("recovery")); }

private slots:
    // Qt runs this before every test: a fresh pair of identical files in a new temp folder.
    void init()
    {
        QVERIFY(directory_.isValid());
        kept_ = writeFile(directory_.filePath(QStringLiteral("keep.txt")), "same contents");
        marked_ = writeFile(directory_.filePath(QStringLiteral("dup.txt")), "same contents");

        DiskLens::DuplicateGroup group;
        group.sizeBytes = kept_.sizeBytes;
        group.files = {kept_, marked_};
        groups_ = {group};
    }

    // Qt runs this after every test so each one starts from an empty folder.
    void cleanup()
    {
        QDir(directory_.path()).removeRecursively();
        QDir().mkpath(directory_.path());
    }

    void movesMarkedFileAndKeepsCopy()
    {
        DiskLens::RecoveryService service(recoveryRoot());
        auto record = service.buildPlan(groups_, {marked_.absolutePath});
        QCOMPARE(record.items.size(), 1);

        service.execute(record);

        const auto &item = record.items.first();
        QCOMPARE(item.status, DiskLens::RecoveryStatus::Moved);
        QCOMPARE(QFileInfo(item.recoveryPath).fileName(), QStringLiteral("0001_dup.txt"));
        QCOMPARE(QFileInfo(item.recoveryPath).dir().dirName(), record.id.toString(QUuid::WithoutBraces));
        QVERIFY(!QFileInfo::exists(marked_.absolutePath));
        QCOMPARE(readFile(item.recoveryPath), QByteArray("same contents"));
        QCOMPARE(readFile(kept_.absolutePath), QByteArray("same contents"));
    }

    void skipsSourceChangedAfterPlan()
    {
        DiskLens::RecoveryService service(recoveryRoot());
        auto record = service.buildPlan(groups_, {marked_.absolutePath});

        writeFile(marked_.absolutePath, "edited after the scan");
        service.execute(record);

        QCOMPARE(record.items.first().status, DiskLens::RecoveryStatus::Skipped);
        QCOMPARE(readFile(marked_.absolutePath), QByteArray("edited after the scan"));
        QVERIFY(!QFileInfo::exists(record.items.first().recoveryPath));
    }

    void skipsWhenKeptCopyChanged()
    {
        DiskLens::RecoveryService service(recoveryRoot());
        auto record = service.buildPlan(groups_, {marked_.absolutePath});

        writeFile(kept_.absolutePath, "the kept copy was edited");
        service.execute(record);

        QCOMPARE(record.items.first().status, DiskLens::RecoveryStatus::Skipped);
        QCOMPARE(readFile(marked_.absolutePath), QByteArray("same contents"));
    }

    void neverOverwritesExistingDestination()
    {
        DiskLens::RecoveryService service(recoveryRoot());
        auto record = service.buildPlan(groups_, {marked_.absolutePath});

        const QString destination = record.items.first().recoveryPath;
        QVERIFY(QDir().mkpath(QFileInfo(destination).absolutePath()));
        writeFile(destination, "already here");

        service.execute(record);

        QCOMPARE(record.items.first().status, DiskLens::RecoveryStatus::Failed);
        QCOMPARE(readFile(destination), QByteArray("already here"));
        QCOMPARE(readFile(marked_.absolutePath), QByteArray("same contents"));
    }

    void storeRoundTripsRecord()
    {
        DiskLens::RecoveryService service(recoveryRoot());
        auto record = service.buildPlan(groups_, {marked_.absolutePath});
        record.items.first().status = DiskLens::RecoveryStatus::Skipped;
        record.items.first().message = QStringLiteral("Changed since scan.");

        const DiskLens::OperationStore store(directory_.filePath(QStringLiteral("operations")));
        QString error;
        QVERIFY2(store.save(record, &error), qPrintable(error));

        const auto loaded = store.load(record.id);
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->id, record.id);
        QCOMPARE(loaded->createdAt, record.createdAt);
        QCOMPARE(loaded->items.size(), 1);

        const auto &before = record.items.first();
        const auto &after = loaded->items.first();
        QCOMPARE(after.originalPath, before.originalPath);
        QCOMPARE(after.recoveryPath, before.recoveryPath);
        QCOMPARE(after.sizeBytes, before.sizeBytes);
        QCOMPARE(after.status, before.status);
        QCOMPARE(after.message, before.message);
    }
};

QTEST_GUILESS_MAIN(RecoveryServiceTests)

#include "RecoveryServiceTests.moc"