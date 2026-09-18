#include "StorageTree.h"

#include <utility>

#include <QFileInfo>
#include <QHash>

namespace DiskLens
{

    namespace
    {
        StorageNode *addChild(StorageNode *parent, const FileEntry &entry)
        {
            auto child = std::make_unique<StorageNode>();
            child->entry = entry;
            child->parent = parent;

            StorageNode *childPtr = child.get();
            parent->children.push_back(std::move(child));
            return childPtr;
        }

        StorageNode *ensureDirectory(const QString &path, QHash<QString, StorageNode *> &nodesByPath)
        {

            // Checks whether the directory node exists. If so, it return it.
            if (StorageNode *existing = nodesByPath.value(path, nullptr))
                return existing;

            // Stop if moving to the parent does not change the path;
            const QString parentPath = QFileInfo(path).absolutePath();
            if (parentPath == path)
                return nullptr;

            // Recursive call that builds missing parent folders first.
            StorageNode *parent = ensureDirectory(parentPath, nodesByPath);
            if (!parent)
                return nullptr;

            FileEntry entry;
            entry.absolutePath = path;
            entry.name = QFileInfo(path).fileName();
            entry.isDirectory = true;

            StorageNode *node = addChild(parent, entry);
            nodesByPath.insert(path, node);
            return node;
        }

        void calculateTotals(StorageNode *node)
        {
            node->totalSizeBytes = 0;
            node->fileCount = 0;

            if (node->entry.isSymbolicLink)
                return;

            if (!node->entry.isDirectory)
            {
                node->totalSizeBytes = node->entry.sizeBytes;
                node->fileCount = 1;
                return;
            }

            for (const auto &child : node->children)
            {
                calculateTotals(child.get());
                node->totalSizeBytes += child->totalSizeBytes;
                node->fileCount += child->fileCount;
            }
        }
    }

    void StorageTree::build(const ScanResult &result)
    {
        root_.reset();

        if (result.rootPath.isEmpty())
            return;

        root_ = std::make_unique<StorageNode>();
        root_->entry.absolutePath = result.rootPath;
        root_->entry.name = QFileInfo(result.rootPath).fileName();

        if (root_->entry.name.isEmpty())
        {
            root_->entry.name = result.rootPath; // e.g. D:/
        }

        root_->entry.isDirectory = true;

        QHash<QString, StorageNode *> nodesByPath;
        nodesByPath.insert(result.rootPath, root_.get());

        for (const FileEntry &entry : result.entries)
        {
            if (!entry.isDirectory || entry.isSymbolicLink)
                continue;

            StorageNode *node = ensureDirectory(entry.absolutePath, nodesByPath);
            if (node)
                node->entry = entry;
        }

        for (const FileEntry &entry : result.entries)
        {
            if (entry.isDirectory && !entry.isSymbolicLink)
                continue;

            const QString parentPath = QFileInfo(entry.absolutePath).absolutePath();
            StorageNode *parent = ensureDirectory(parentPath, nodesByPath);
            if (!parent || nodesByPath.contains(entry.absolutePath))
                continue;

            StorageNode *node = addChild(parent, entry);
            nodesByPath.insert(entry.absolutePath, node);
        }

        calculateTotals(root_.get());
    }

    const StorageNode *StorageTree::root() const noexcept
    {
        return root_.get();
    }

} // namespace DiskLens