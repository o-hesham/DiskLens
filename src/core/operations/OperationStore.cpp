#include "OperationStore.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QFile>

namespace DiskLens
{

    namespace
    {
        QString statusToString(RecoveryStatus status)
        {
            switch (status)
            {
            case RecoveryStatus::Pending:
                return QStringLiteral("pending");
            case RecoveryStatus::Moved:
                return QStringLiteral("moved");
            case RecoveryStatus::Skipped:
                return QStringLiteral("skipped");
            case RecoveryStatus::Failed:
                return QStringLiteral("failed");
            }
            return QStringLiteral("failed");
        }

        RecoveryStatus statusFromString(const QString &text)
        {
            if (text == QStringLiteral("moved"))
                return RecoveryStatus::Moved;
            if (text == QStringLiteral("skipped"))
                return RecoveryStatus::Skipped;
            if (text == QStringLiteral("failed"))
                return RecoveryStatus::Failed;
            return RecoveryStatus::Pending;
        }

    } // namespace

    OperationStore::OperationStore(QString directory)
        : directory_(directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/operations") : std::move(directory))
    {
    }

    bool OperationStore::save(const OperationRecord &record, QString *errorMessage) const
    {
        QJsonArray items;
        for (const auto &item : record.items)
        {
            items.append(QJsonObject{
                {QStringLiteral("originalPath"), item.originalPath},
                {QStringLiteral("recoveryPath"), item.recoveryPath},
                {QStringLiteral("sizeBytes"), item.sizeBytes},
                {QStringLiteral("lastModified"), item.lastModified.toUTC().toString(Qt::ISODateWithMs)},
                {QStringLiteral("digest"), QString::fromLatin1(item.digest.toHex())},
                {QStringLiteral("status"), statusToString(item.status)},
                {QStringLiteral("message"), item.message}});
        }

        const QJsonObject root{
            {QStringLiteral("id"), record.id.toString(QUuid::WithoutBraces)},
            {QStringLiteral("createdAt"), record.createdAt.toString(Qt::ISODateWithMs)},
            {QStringLiteral("items"), items}};

        QSaveFile file(filePathFor(record.id));
        const bool ok = QDir().mkpath(directory_) && file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(root).toJson()) >= 0 && file.commit();

        if (!ok && errorMessage)
            *errorMessage = file.errorString();

        return ok;
    }

    std::optional<OperationRecord> OperationStore::load(const QUuid &id) const
    {
        QFile file(filePathFor(id));
        if (!file.open(QIODevice::ReadOnly))
            return std::nullopt;

        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject())
            return std::nullopt;

        const QJsonObject root = document.object();

        OperationRecord record;
        record.id = QUuid::fromString(root.value(QStringLiteral("id")).toString());
        record.createdAt = QDateTime::fromString(root.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs);

        for (const auto &value : root.value(QStringLiteral("items")).toArray())
        {
            const QJsonObject object = value.toObject();

            RecoveryItem item;
            item.originalPath = object.value(QStringLiteral("originalPath")).toString();
            item.recoveryPath = object.value(QStringLiteral("recoveryPath")).toString();
            item.sizeBytes = object.value(QStringLiteral("sizeBytes")).toInteger();
            item.lastModified = QDateTime::fromString(object.value(QStringLiteral("lastModified")).toString(), Qt::ISODateWithMs);
            item.digest = QByteArray::fromHex(object.value(QStringLiteral("digest")).toString().toLatin1());
            item.status = statusFromString(object.value(QStringLiteral("status")).toString());
            item.message = object.value(QStringLiteral("message")).toString();

            record.items.append(item);
        }

        return record;
    }

    QString OperationStore::filePathFor(const QUuid &id) const
    {
        return QDir(directory_).filePath(id.toString(QUuid::WithoutBraces) + QStringLiteral(".json"));
    }

} // namespace DiskLens
