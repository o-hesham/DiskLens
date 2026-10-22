# DiskLens recovery move (slice 1) design

Date: 2026-10-22
Parent spec: `2026-09-10-disklens-design.md` (Milestone 7 — Recovery and cleanup operations)

## Goal

Let the user move the duplicate files they marked in the Duplicates tab into an app-managed recovery area, after a preview and explicit confirmation, with a persisted record of every move.

## Scope

In scope (slice 1):

- Preview of the planned move.
- Rename-only move into a per-drive recovery folder.
- Pre-move safety checks.
- Per-file results.
- A persisted operation record written before and after the move.
- Scanner exclusion of recovery folders.

Out of scope (slice 2 and later):

- Restore and conflict handling on restore.
- Recovery-history view.
- Purging the recovery area.
- Cancellation and background execution of moves.
- Native Recycle Bin integration.

## Decisions

1. **Slice order:** preview, move, and record first. Restore comes next.
2. **Recovery location:** one folder per drive, `<drive root>/.DiskLens Recovery/`. Moves are same-drive renames only; there is no copy fallback. Records are kept centrally in `QStandardPaths::AppLocalDataLocation`.
3. **Layout:** one flat folder per operation, `<drive root>/.DiskLens Recovery/<operation id>/`. Files are named `<index>_<original file name>`, with a zero-padded four-digit index starting at `0001`. Original paths live only in the record. Names never include folder paths, which prevents both collisions and path traversal.
4. **Pre-move check:** compare size and modified time against the scan for the file being moved, and for at least one unmarked copy in its group. If either check fails, skip the file. There is no re-hashing.

## Core components (`src/core/operations/`)

### `OperationRecord.h`

- `RecoveryItem`: `originalPath`, `recoveryPath`, `sizeBytes`, `lastModified`, `digest`, `status`, `message`.
- `RecoveryItem::status` is one of `Pending`, `Moved`, `Skipped`, `Failed`. `message` explains a skip or failure.
- `OperationRecord`: `id` (`QUuid`), `createdAt`, `QVector<RecoveryItem> items`.

### `RecoveryService`

- Takes an optional recovery-root override. The app leaves it empty, so the drive root from `QStorageInfo(path).rootPath()` is used. Tests set it to a folder inside a `QTemporaryDir`.
- `buildPlan(groups, markedPaths)` returns an `OperationRecord` with every item `Pending` and its recovery path computed. It makes no filesystem writes. It also keeps the unmarked copies of each group in memory for the pre-move check; they are not saved in the record.
- `execute(record)` handles each item in turn:
  - Run the pre-move check.
  - Create the operation folder.
  - Check that the destination does not exist (if it does, the item is `Failed`), then call `std::filesystem::rename` with an `std::error_code`. Paths come from `QFileInfo::filesystemFilePath()`. Neither `QFile::rename` nor `QDir::rename` is used, because the Qt 6 documentation says both fall back to copy and remove when a rename fails. `std::filesystem::rename` never copies, but it may replace an existing target, which is why the existence check comes first. The destination is inside a folder unique to this operation, so nothing else should be writing to it between the check and the rename.
  - Set each item's `status` and `message`.

### `OperationStore`

- Saves one JSON file per operation at `AppLocalDataLocation/operations/<id>.json`, written with `QSaveFile`.
- Saves twice: before any move (all items `Pending`) and after the move (final statuses).
- Loading is needed now only for the round-trip test, and later for restore.

### Scanner change

`FilesystemScanner` skips folders named `.DiskLens Recovery`.

## UI flow

- New action `Move Marked to Recovery…` (`moveToRecoveryAction_`) on the toolbar and in the Scan menu. It is enabled only while at least one file is marked; the existing `itemChanged` handler that computes `markedCount` updates it.
- When clicked:
  1. Collect the marked paths from `duplicatesModel_`, the source model, not the filtered proxy.
  2. Build the plan from `duplicateResult_.groups`.
  3. Show `CleanupPreviewDialog` (`src/ui/dialogs/`). It has:
     - a header with the file count and total size, for example `Move 12 files (1.4 GiB) to recovery`
     - a table with the columns Name, Size, From, To
     - a note that files are moved, not deleted, and that in-app restore comes in a later version
     - `Move` and `Cancel` buttons, with `Cancel` as the default button
- On confirm:
  1. Save the record.
  2. Execute the plan on the UI thread, with a wait cursor.
  3. Save the record again.
  4. Show a results `QMessageBox` with a summary (`Moved N, skipped N, failed N`). Its detailed text lists each skipped or failed file and the reason.
- After the move:
  - Remove the rows of moved files from `duplicatesModel_`.
  - Remove any group left with fewer than two files.
  - Recompute the tab title and the action's enabled state.
  - Set the status message `Some files were moved. Rescan to refresh sizes.` The Files and Storage Tree views are not patched.

## Error handling

| Situation | Result |
|---|---|
| The first record save fails | The whole operation is aborted, no files are moved, and an error dialog is shown. |
| The source's size or modified time changed | `Skipped`: "Changed since scan" |
| No kept copy matches its scan | `Skipped`: "No unchanged copy left to keep" |
| The recovery folder can't be created on a drive | `Failed` for that drive's items; other drives continue. |
| The rename fails | `Failed`, with the `std::error_code` message |
| The destination already exists | `Failed`; the destination is never overwritten. |
| The second record save fails | A warning is shown; the pre-move record still lists every item. |

## Testing

There is one new Qt Test target, `RecoveryServiceTests`. Every test uses a `QTemporaryDir` and the recovery-root override.

1. A marked file moves to `<id>/0001_<name>`, its kept copy is untouched, and its status is `Moved`.
2. Changing the source after planning gives `Skipped`, and the file is not moved.
3. Changing the kept copy after planning gives `Skipped`.
4. When a file already exists at the destination, the item is `Failed` and both files keep their contents.
5. An `OperationStore` save and load round trip returns an equal record.

Manual check: mark files in the app, preview them, move them, and confirm that the files are in `<drive>/.DiskLens Recovery/<id>/`, the record JSON exists, the results dialog is correct, and the Duplicates tab has been updated.

## Unverified assumption

- A standard Windows user can create a folder (but not a file) at the root of `C:\`. This has not been checked on the target machine. If it is false, the folder-creation failure path reports it per file.
