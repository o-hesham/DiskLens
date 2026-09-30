# DiskLens Duplicate Detection Design Specification

## Purpose

This milestone adds read-only exact-duplicate detection to DiskLens. It operates on the files produced by a completed filesystem scan, stays responsive on large scan roots, and reports only groups whose contents have been fully verified. It does not move, delete, or modify user files.

Success means that users can explicitly start duplicate analysis, monitor or cancel it, inspect confirmed duplicate groups, and understand the amount of space that could be recovered by retaining one physical copy from each group.

## Scope

The milestone includes:

- User-triggered duplicate analysis after a successful scan.
- Filtering of ineligible scan entries.
- Staged candidate reduction and exact content verification.
- Hard-link-aware recoverable-space calculations.
- Background execution, progress, cancellation, and per-file issue reporting.
- A hierarchical duplicate-results model and view.
- Details for groups and individual files.
- Automated core tests and manual UI verification.

The milestone excludes:

- Moving, deleting, or selecting files for cleanup.
- Automatic keep policies.
- Persistent hash caches or scan history.
- Similar-image, similar-video, or filename-based matching.
- Parallel or adaptive hashing.

## Eligibility rules

Duplicate analysis accepts the entries from the latest completed scan. An entry is eligible only when it is a regular, non-empty, non-symbolic-link file. Directories, zero-byte files, and symbolic links are ignored and do not appear as errors.

The analysis is invalidated when a new scan begins. Starting a new scan clears existing duplicate results and disables duplicate analysis until that scan completes successfully with at least one eligible file.

## Analysis pipeline

The duplicate finder executes the following stages sequentially on one background worker:

1. **Size grouping:** group eligible files by their exact byte size and discard groups containing only one path.
2. **Sample hashing:** compute SHA-256 over the first and last 64 KiB of each candidate file. For files smaller than 128 KiB, hash their contents once without overlapping reads. Discard sample groups containing only one path.
3. **Full hashing:** read each surviving file in bounded chunks and compute a full SHA-256 digest. Discard full-hash groups containing only one path.
4. **Change validation:** compare the file's size and last-modified time immediately before and after content processing. A file that changed, vanished, or became unreadable is excluded and recorded as an issue.
5. **Direct comparison:** compare full-hash matches byte for byte in bounded chunks. Only byte-identical files enter the same confirmed group.
6. **Physical-file collapse:** use `std::filesystem::equivalent` to identify paths that refer to the same physical file. Hard-link aliases are represented by one physical copy and do not inflate copy counts or recoverable bytes.

SHA-256 narrows candidates efficiently; the final direct comparison provides exact byte equality rather than relying on collision probability. The algorithm does not read entire files into memory.

## Result semantics

A duplicate group contains at least two distinct physical files whose contents are byte-identical. Each group records:

- A stable list of its file records.
- Exact size per physical copy.
- Full SHA-256 digest for display and diagnostics.
- Physical copy count.
- Ignored hard-link alias count.
- Recoverable bytes.

Recoverable bytes are calculated as:

```text
file size x (physical copy count - 1)
```

This is an estimate for a future cleanup workflow that retains one physical copy. This milestone performs no cleanup.

The overall result records a terminal status, confirmed groups, issues, ignored hard-link aliases, total duplicate physical copies, and total recoverable bytes. Cancelled analysis returns no confirmed groups, preventing incomplete work from being presented as final.

## Components and responsibilities

### DuplicateFinder

`DuplicateFinder` belongs to the core analysis layer. It owns the staged algorithm and has no widget dependencies. It receives scan entries, a shared cancellation token, and a progress callback. It returns a `DuplicateResult` value.

### Duplicate result types

Core value types describe `DuplicateGroup`, duplicate file records, terminal status, progress phase, and per-file issues. Issue categories distinguish missing, unreadable, changed, and physical-identity-check failures. Messages remain suitable for UI display but do not control program flow.

### DuplicateJob

`DuplicateJob` follows the existing `ScanJob` pattern. It runs `DuplicateFinder` on a dedicated `QThread`, translates callbacks into queued progress signals, and emits one terminal result. It shares the existing cooperative `CancellationToken` abstraction.

### DuplicateTreeModel

`DuplicateTreeModel` is a read-only hierarchical `QAbstractItemModel`. Top-level rows represent duplicate groups; child rows represent physical files. It borrows or owns only stable result data whose lifetime exceeds the model indexes, and it resets before that data is replaced.

### MainWindow coordination

`MainWindow` owns the active duplicate thread, job, cancellation token, result, model, view, and actions. It prevents scanning and duplicate analysis from running simultaneously. The existing scan-specific cancellation action becomes a general **Cancel** action whose behavior depends on the active operation.

## User interface

The Scan menu and main toolbar gain a **Find Duplicates** action. It is disabled until the latest successful scan contains eligible files, disabled again while any background operation runs, and re-enabled after cancellation or completion when the scan remains valid.

The content area gains a **Duplicates** tab using a `QTreeView`. A top-level group row summarizes copy count, size per copy, and recoverable bytes. Expanding the group shows individual files with these columns:

- Name
- Folder
- Size
- Modified

Duplicate analysis progress appears in the status bar with a phase and useful work count, for example:

```text
Sampling files: 18 / 42
Verifying full contents: 7 / 12
```

During full hashing or direct comparison, the status may also show bytes processed for the current file. Progress is not presented as a misleading single percentage when later-stage totals are not yet known.

On successful completion, the application switches to the Duplicates tab. Selecting a group shows its physical copy count, ignored hard-link aliases, size per copy, digest, and recoverable bytes. Selecting a file shows its full path, size, modified time, and digest.

## Cancellation and concurrency

The first implementation uses one sequential worker thread. This keeps disk access predictable across HDDs and SSDs, limits memory usage, and simplifies deterministic progress and cancellation. Parallel hashing is deferred until measurements demonstrate a need.

Cancellation is checked between files and between bounded read chunks. Once requested, the finder stops promptly, returns a cancelled status with issues accumulated for diagnostics, and returns no confirmed groups. The worker thread follows the same cleanup lifecycle as the existing scan thread.

## Errors and changing files

An error affecting one file does not stop unrelated candidates. Missing, unreadable, changed, and identity-unverifiable paths are skipped and recorded. A fatal setup or internal error terminates the job cleanly and does not overwrite the last valid duplicate result.

If `std::filesystem::equivalent` cannot establish physical identity for a path in a prospective duplicate group, that path is excluded rather than counted as independently reclaimable space. Hard-link aliases are not errors, but their ignored count is reported.

Metadata checks reduce races with changing files but cannot lock arbitrary user files across every supported filesystem. Results describe the successfully verified snapshot observed during analysis. Future cleanup must revalidate files immediately before acting.

## Cross-platform requirements

The analysis core uses Qt Core and C++17 standard-library facilities only. Paths pass through `std::filesystem` using native path conversion, and filesystem operations use non-throwing `std::error_code` overloads where available. Windows is validated first; the same core tests must pass on macOS and Linux before those packages are released.

No platform-specific hard-link API is required for this milestone. If standard-library equivalence proves unreliable on a supported filesystem, a later platform adapter may replace that implementation without changing duplicate-result semantics.

## Verification strategy

A focused core test target will use Qt Test and `QTemporaryDir`. Tests create only temporary fixture files and never operate on user scan locations. The required cases are:

- Equal sizes with different content are rejected.
- Identical content is grouped.
- Zero-byte files and symbolic links are ignored.
- Three identical physical files produce the correct recoverable-byte total.
- Hard-link aliases count as one physical copy when hard links are supported by the test filesystem.
- Missing, changed, and unreadable files are skipped and classified.
- Cancellation returns no partial confirmed groups.
- Sample-hash matches still require full hashing and direct comparison.

Model tests will verify parent/child indexing, columns, display values, and model reset behavior using deterministic result fixtures. Manual verification will cover action enablement, background responsiveness, progress text, cancellation, tab switching, sorting where enabled, and details-panel selection.

## Acceptance criteria

The milestone is complete when:

- Duplicate analysis never runs on the GUI thread.
- Users explicitly start and can cancel analysis.
- Only distinct physical files with directly compared identical bytes appear as duplicates.
- Zero-byte files and symbolic links do not clutter results.
- Hard-link aliases do not inflate recoverable-space estimates.
- File-level failures do not abort unrelated analysis and are summarized accurately.
- Cancellation does not display partial groups as confirmed results.
- Starting a new scan clears stale duplicate results.
- Core and model tests pass, the application builds, and the documented manual UI checks pass.

