#include "StorageTreeModel.h"
#include "../../core/analysis/StorageTree.h"

#include <QLocale>

namespace DiskLens
{

    StorageTreeModel::StorageTreeModel(QObject *parent)
        : QAbstractItemModel(parent) {}

    QModelIndex StorageTreeModel::index(int row, int column, const QModelIndex &parentIndex) const
    {
        if (!root_ || row < 0 || column < 0 || column >= ColumnCount)
            return {};

        if (!parentIndex.isValid())
            return row == 0 ? createIndex(0, column, root_) : QModelIndex{};

        if (parentIndex.column() != NameColumn)
            return {};

        const auto *parentNode =
            static_cast<const StorageNode *>(parentIndex.internalPointer());

        if (row >= static_cast<int>(parentNode->children.size()))
            return {};

        return createIndex(row, column, parentNode->children[row].get());
    }

    QModelIndex StorageTreeModel::parent(const QModelIndex &childIndex) const
    {
        if (!root_ || !childIndex.isValid())
            return {};

        const auto *node = static_cast<const StorageNode *>(childIndex.internalPointer());
        const StorageNode *parentNode = node->parent;

        if (!parentNode)
            return {}; // the root has no parent
        if (parentNode == root_)
            return createIndex(0, NameColumn, root_);

        const StorageNode *grandparent = parentNode->parent;
        if (!grandparent)
            return {};

        for (int row = 0; row < static_cast<int>(grandparent->children.size()); ++row)
        {
            if (grandparent->children[row].get() == parentNode)
                return createIndex(row, NameColumn, parentNode);
        }
        return {};
    }

    void StorageTreeModel::setRoot(const StorageNode *root)
    {
        beginResetModel();
        root_ = root;
        endResetModel();
    }

    int StorageTreeModel::columnCount(const QModelIndex &) const
    {
        return ColumnCount;
    }

    QVariant StorageTreeModel::data(const QModelIndex &modelIndex, int role) const
    {
        if (!root_ || !modelIndex.isValid() || role != Qt::DisplayRole)
            return {};

        const auto *node = static_cast<const StorageNode *>(modelIndex.internalPointer());

        switch (modelIndex.column())
        {
        case NameColumn:
            return node->entry.name;

        case SizeColumn:
            if (node->entry.isSymbolicLink)
                return QStringLiteral("—");
            return QLocale().formattedDataSize(node->totalSizeBytes);

        case FileCountColumn:
            if (!node->entry.isDirectory || node->entry.isSymbolicLink)
                return QStringLiteral("—");
            return QLocale().toString(static_cast<qlonglong>(node->fileCount));

        default:
            return {};
        }
    }

    QVariant StorageTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
            return QAbstractItemModel::headerData(section, orientation, role);

        switch (section)
        {
        case NameColumn:
            return QStringLiteral("Name");
        case SizeColumn:
            return QStringLiteral("Size");
        case FileCountColumn:
            return QStringLiteral("Files");
        default:
            return {};
        }
    }

    int StorageTreeModel::rowCount(const QModelIndex &parentIndex) const
    {
        if (!root_)
            return 0;
        if (!parentIndex.isValid())
            return 1; // the scan root
        if (parentIndex.column() != NameColumn)
            return 0;

        const auto *node =
            static_cast<const StorageNode *>(parentIndex.internalPointer());
        return static_cast<int>(node->children.size());
    }

} // namespace DiskLens