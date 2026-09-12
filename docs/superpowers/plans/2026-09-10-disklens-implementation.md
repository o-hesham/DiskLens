# DiskLens Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build DiskLens as a cross-platform Qt desktop application that visualizes disk usage, finds exact duplicate files, recommends safe cleanup, and supports reversible file operations.

**Architecture:** Keep filesystem access, analysis, persistence, platform integration, background jobs, and presentation as separate modules. The core scanner and analysis services must not depend on widgets; Qt models and views consume their results. Windows is the first tested and packaged platform, while platform-specific behavior is isolated behind adapters.

**Tech Stack:** Qt 6, CMake, C++, Qt Widgets, Qt Core, Qt Gui, Qt Test, and the standard C++ library. Avoid third-party dependencies until a concrete requirement justifies one.

**Spec:** `docs/superpowers/specs/2026-09-10-disklens-design.md`

## Global Constraints

- Keep the core implementation platform-independent; isolate OS-specific behavior behind platform services.
- Default all analysis to read-only operation.
- Do not follow symbolic links by default.
- Never overwrite files during move or restore.
- Move files to an app-managed recovery area before considering native Trash/Recycle Bin integration.
- Keep filesystem traversal, hashing, and file operations off the GUI thread.
- Treat files, paths, permissions, and timestamps as unreliable external input.
- Report partial success and partial failure accurately.
- Test destructive workflows only with temporary fixture directories.
- Do not add automatic deletion; every cleanup action requires an explicit user confirmation.

## File and module structure

The empty repository will be organized as follows:

- `CMakeLists.txt` — project configuration, targets, Qt modules, tests, and install rules.
- `src/main.cpp` — application entry point and dependency assembly.
- `src/app/` — application startup, main window, commands, settings wiring, and lifecycle.
- `src/core/filesystem/` — filesystem entries, directory enumeration, metadata, symlink policy, and filesystem errors.
- `src/core/analysis/` — hierarchy aggregation, file-type classification, hashing, duplicate groups, and recommendations.
- `src/core/operations/` — recovery moves, restore operations, operation records, and conflict handling.
- `src/core/platform/` — portable interfaces plus Windows, macOS, and Linux adapters.
- `src/core/persistence/` — settings, exclusions, profiles, scan summaries, and operation history.
- `src/core/jobs/` — cancellable background work, progress, and job state.
- `src/ui/models/` — Qt models that expose core data to widgets.
- `src/ui/views/` — treemap, tree, table, details, and summary views.
- `src/ui/dialogs/` — scan configuration, cleanup preview, recovery history, settings, and errors.
- `tests/unit/` — deterministic tests for core modules.
- `tests/integration/` — temporary-directory tests for scanner and recovery behavior.
- `resources/` — icons, themes, and application resources.
- `docs/` — product specification, implementation plan, user-facing notes, and release notes.

## Action items

### Task 1: Establish the project baseline

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/main.cpp`
- Create: `src/app/AppVersion.h`
- Create: `tests/CMakeLists.txt`
- Create: `tests/unit/smoke_test.cpp`
- Create: `.gitignore`

**Interfaces:**
- Produces a desktop executable target and a test target.
- Produces a single application-version source used by the window and About dialog.

- [ ] Confirm the installed Qt 6 toolchain, CMake, compiler, and generator available on the development machine.
- [ ] Configure a CMake project with Qt Core, Gui, Widgets, and Test dependencies.
- [ ] Define separate application and test targets so core tests can run without launching the main window.
- [ ] Add the minimum application entry point that creates a `QApplication`, constructs the main window boundary, and enters the event loop.
- [ ] Add a smoke test that verifies the test executable starts and the version value is non-empty.
- [ ] Add build output and IDE-generated files to `.gitignore`.
- [ ] Configure a Debug build and run the smoke test before adding product features.

**Validation:**
- Configure: `cmake -S . -B build`
- Build: `cmake --build build --config Debug`
- Test: `ctest --test-dir build -C Debug --output-on-failure`

### Task 2: Build the application shell and settings boundary

**Files:**
- Create: `src/app/MainWindow.h` and `src/app/MainWindow.cpp`
- Create: `src/app/AppSettings.h` and `src/app/AppSettings.cpp`
- Create: `src/app/AppCommands.h` and `src/app/AppCommands.cpp`
- Create: `resources/resources.qrc`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/unit/app_settings_test.cpp`

**Interfaces:**
- `MainWindow` owns the top-level layout and exposes actions for selecting a scan location, starting a scan, cancelling a scan, opening settings, and viewing history.
- `AppSettings` reads and writes user settings through Qt’s platform-appropriate settings location.
- `AppCommands` defines named actions so menus, toolbar buttons, keyboard shortcuts, and future command routing use the same behavior.

- [ ] Create the main window layout with menu bar, toolbar, status bar, central content area, and a safe empty state.
- [ ] Add disabled command placeholders for scan, cancel, duplicate analysis, cleanup, restore, and settings until their services exist.
- [ ] Add the DiskLens name, version, About action, and initial light/dark theme selection.
- [ ] Store theme preference and the last selected scan locations through `AppSettings`.
- [ ] Ensure settings failures are non-fatal and visible through a user-facing warning.
- [ ] Add keyboard shortcuts for opening a scan, cancelling work, refreshing, and opening settings.
- [ ] Test settings round-tripping in Qt test mode so tests do not touch the real user profile.

**Validation:**
- Launch the Debug application and verify the empty state, menus, theme choice, About dialog, and restart persistence manually.
- Run the unit test target and confirm the test-mode settings directory is used.

### Task 3: Define filesystem domain objects and policies

**Files:**
- Create: `src/core/filesystem/FileEntry.h`
- Create: `src/core/filesystem/DirectoryEntry.h`
- Create: `src/core/filesystem/VolumeInfo.h`
- Create: `src/core/filesystem/FilesystemError.h`
- Create: `src/core/filesystem/ScanScope.h`
- Create: `src/core/filesystem/SymlinkPolicy.h`
- Create: `tests/unit/filesystem_domain_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `FileEntry` represents one observed file and contains its path, size, timestamps, type information, permissions, and optional content hash.
- `DirectoryEntry` represents a directory with child entries, aggregate byte count, file count, and scan status.
- `ScanScope` contains selected roots, exclusions, include patterns, age/size filters, and symlink policy.
- `FilesystemError` classifies inaccessible, missing, changing, unsupported, and unknown failures without throwing UI-specific text into the core.

- [ ] Define stable entry identifiers that remain valid while a scan result is displayed.
- [ ] Define how logical size, zero-byte files, directories, and unavailable metadata are represented.
- [ ] Define path normalization rules without assuming Windows separators in the core.
- [ ] Define the default policy of not following symlinks and the representation of skipped links.
- [ ] Define scan-scope matching for roots, excluded directories, excluded file patterns, and protected paths.
- [ ] Test equality, aggregation inputs, path normalization, and policy decisions with platform-neutral fixtures.

**Validation:**
- Run domain unit tests on paths containing spaces, Unicode characters, long names, empty files, and missing metadata.
- Confirm the domain layer has no dependency on Qt Widgets.

### Task 4: Implement the cancellable filesystem scanner

**Files:**
- Create: `src/core/filesystem/FilesystemScanner.h` and `src/core/filesystem/FilesystemScanner.cpp`
- Create: `src/core/jobs/JobState.h`
- Create: `src/core/jobs/ProgressInfo.h`
- Create: `src/core/jobs/CancellationToken.h`
- Create: `src/core/jobs/ScanJob.h` and `src/core/jobs/ScanJob.cpp`
- Create: `tests/integration/filesystem_scanner_test.cpp`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `FilesystemScanner` accepts a `ScanScope` and produces entries plus structured errors.
- `ScanJob` runs the scanner outside the GUI thread and reports state, progress, partial results, and completion.
- `CancellationToken` allows cooperative cancellation at directory and file boundaries.

- [ ] Enumerate directories using Qt filesystem APIs while avoiding widget or model code in the scanner.
- [ ] Collect metadata defensively and continue scanning when an individual file disappears or becomes inaccessible.
- [ ] Apply root, exclusion, pattern, symlink, and protected-path policies during traversal.
- [ ] Add progress fields for visited entries, discovered files, bytes, current path, and unknown totals.
- [ ] Move the scan job to a worker context with a safe signal/slot boundary to the main thread.
- [ ] Ensure cancellation stops new work and returns a truthful cancelled result.
- [ ] Connect the main window’s scan and cancel commands to the job service.
- [ ] Test nested directories, permissions, disappearing files, symlink loops, cancellation, and partial failures using temporary directories.

**Validation:**
- Scan a small fixture tree and verify counts, sizes, exclusions, and errors.
- Start a scan of a large temporary fixture, confirm the UI remains responsive, then cancel it and inspect the reported state.

### Task 5: Add hierarchy aggregation and Qt data models

**Files:**
- Create: `src/core/analysis/StorageTree.h` and `src/core/analysis/StorageTree.cpp`
- Create: `src/core/analysis/FileTypeClassifier.h` and `src/core/analysis/FileTypeClassifier.cpp`
- Create: `src/core/analysis/ScanSummary.h`
- Create: `src/ui/models/StorageTreeModel.h` and `src/ui/models/StorageTreeModel.cpp`
- Create: `src/ui/models/FileTableModel.h` and `src/ui/models/FileTableModel.cpp`
- Create: `tests/unit/storage_tree_test.cpp`
- Create: `tests/unit/file_type_classifier_test.cpp`

**Interfaces:**
- `StorageTree` consumes scanner entries and exposes aggregate folder/file data.
- `FileTypeClassifier` maps a path to a display category without changing the original extension.
- `StorageTreeModel` exposes hierarchical entries to `QTreeView`.
- `FileTableModel` exposes the selected directory’s files with sortable columns.

- [ ] Build the hierarchy from scan entries and calculate directory totals without double-counting.
- [ ] Add file-type categories for common documents, images, audio, video, archives, applications, temporary files, and unknown files.
- [ ] Expose display roles for name, path, size, type, modified time, and status.
- [ ] Add sorting by name, size, type, and modified time.
- [ ] Add filters for minimum size, extension/category, age, path text, and duplicate status input.
- [ ] Wire the tree and table models into the main window.
- [ ] Test aggregate totals, empty directories, zero-byte files, unknown types, and stable sorting.

**Validation:**
- Verify the tree and table show identical sizes and counts for the same fixture.
- Verify selecting a directory updates the table without rescanning the filesystem.

### Task 6: Implement the treemap visualization

**Files:**
- Create: `src/ui/views/TreemapView.h` and `src/ui/views/TreemapView.cpp`
- Create: `src/ui/views/TreemapLayout.h` and `src/ui/views/TreemapLayout.cpp`
- Create: `src/ui/views/TreemapColorPolicy.h` and `src/ui/views/TreemapColorPolicy.cpp`
- Create: `tests/unit/treemap_layout_test.cpp`
- Modify: `src/app/MainWindow.*`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `TreemapLayout` accepts a directory and child sizes and returns non-overlapping rectangles within a requested viewport.
- `TreemapView` renders a `StorageTree` node, maps pointer/keyboard selection to entry identifiers, and emits navigation requests.
- `TreemapColorPolicy` maps file-type or directory grouping to stable colors.

- [ ] Implement a squarified treemap layout with deterministic output for the same input.
- [ ] Handle zero-size children, tiny rectangles, empty folders, resize events, and deep hierarchies.
- [ ] Add hover tooltips with path, size, percentage, and file count.
- [ ] Add click selection, double-click zoom, breadcrumb navigation, keyboard focus, and back navigation.
- [ ] Synchronize treemap selection with the tree and table models.
- [ ] Add color modes for file type and directory depth.
- [ ] Test rectangle bounds, non-overlap, total-area behavior, zero-size entries, and selection mapping.

**Validation:**
- Compare treemap totals against the model using fixture data.
- Resize the window and navigate through nested folders while confirming selection remains correct.

### Task 7: Add staged exact-duplicate detection

**Files:**
- Create: `src/core/analysis/HashingPolicy.h`
- Create: `src/core/analysis/ContentHasher.h` and `src/core/analysis/ContentHasher.cpp`
- Create: `src/core/analysis/DuplicateGroup.h`
- Create: `src/core/analysis/DuplicateFinder.h` and `src/core/analysis/DuplicateFinder.cpp`
- Create: `src/ui/models/DuplicateModel.h` and `src/ui/models/DuplicateModel.cpp`
- Create: `src/ui/views/DuplicateView.h` and `src/ui/views/DuplicateView.cpp`
- Create: `tests/unit/duplicate_finder_test.cpp`
- Create: `tests/integration/content_hasher_test.cpp`

**Interfaces:**
- `ContentHasher` computes a complete SHA-256 content hash after candidate filtering.
- `DuplicateFinder` accepts scan entries and a hashing policy and emits duplicate groups plus skipped/error records.
- `DuplicateGroup` contains equal-content files, recoverable byte count, and user selections.
- `DuplicateModel` exposes group members and selection policies to the duplicate view.

- [ ] Group candidates by compatible file size before reading full contents.
- [ ] Use a small sample comparison where it reduces unnecessary full hashing, while ensuring complete hashes confirm every reported duplicate.
- [ ] Run hashing as cancellable background work with progress and per-file errors.
- [ ] Detect files that change during hashing and mark their result stale instead of trusting it.
- [ ] Add duplicate groups with keep-newest, keep-oldest, keep-shortest-path, and manual selection policies.
- [ ] Add previews for safe common file types without making preview generation required for detection.
- [ ] Add duplicate filters to the file table and treemap models.
- [ ] Test equal-size different-content files, equal-content different names, zero-byte files, changing files, unreadable files, and cancellation.

**Validation:**
- Use generated fixture files with known content and verify exact group membership.
- Confirm the UI never labels equal-size files as duplicates without complete content confirmation.

### Task 8: Implement cleanup recommendations

**Files:**
- Create: `src/core/analysis/CleanupCandidate.h`
- Create: `src/core/analysis/CleanupRule.h`
- Create: `src/core/analysis/CleanupRules.h` and `src/core/analysis/CleanupRules.cpp`
- Create: `src/core/analysis/ProtectedPathPolicy.h` and `src/core/analysis/ProtectedPathPolicy.cpp`
- Create: `src/ui/models/CleanupModel.h` and `src/ui/models/CleanupModel.cpp`
- Create: `src/ui/views/CleanupView.h` and `src/ui/views/CleanupView.cpp`
- Create: `tests/unit/cleanup_rules_test.cpp`

**Interfaces:**
- `CleanupRule` evaluates one candidate category and returns explainable candidates.
- `CleanupCandidate` contains path, reason, estimated bytes, reversibility, risk level, and selection state.
- `CleanupRules` combines large-file, old-file, temporary, cache, old-download, empty-directory, and duplicate rules.
- `ProtectedPathPolicy` rejects protected or out-of-scope paths before recommendations are displayed.

- [ ] Implement each rule as a separately testable unit with explicit thresholds.
- [ ] Add explanations that identify the exact rule and threshold that produced each candidate.
- [ ] Exclude protected system locations and ambiguous application data by default.
- [ ] Add configurable thresholds for size and age without changing the scan result itself.
- [ ] Show estimated reclaimable space without summing the same file twice across categories.
- [ ] Add selection, category filters, risk indicators, and “show affected files” actions.
- [ ] Test protected paths, recently modified files, duplicate overlap, excluded paths, empty directories, and zero-byte files.

**Validation:**
- Run recommendations against deterministic fixtures and verify every displayed explanation matches the rule that produced it.
- Confirm generating recommendations never changes the fixture filesystem.

### Task 9: Add safe recovery operations

**Files:**
- Create: `src/core/operations/OperationRecord.h`
- Create: `src/core/operations/RecoveryService.h` and `src/core/operations/RecoveryService.cpp`
- Create: `src/core/operations/RestoreService.h` and `src/core/operations/RestoreService.cpp`
- Create: `src/ui/dialogs/CleanupPreviewDialog.h` and `src/ui/dialogs/CleanupPreviewDialog.cpp`
- Create: `src/ui/dialogs/OperationResultDialog.h` and `src/ui/dialogs/OperationResultDialog.cpp`
- Create: `tests/integration/recovery_service_test.cpp`

**Interfaces:**
- `RecoveryService` previews and moves selected files into an app-managed recovery area.
- `RestoreService` restores a recorded move only when the destination is safe and conflict-free.
- `OperationRecord` stores operation identifier, original path, recovery path, size, timestamps, optional hash, result, and error.

- [ ] Design a recovery-directory layout that preserves original-path context without allowing path traversal or collisions.
- [ ] Preview exact file count, estimated bytes, source paths, destination paths, and irreversible-risk warnings before confirmation.
- [ ] Re-check existence, permissions, and file identity immediately before each move.
- [ ] Never overwrite a recovery or restore destination; generate a visible conflict result instead.
- [ ] Support cooperative cancellation and per-file success, skip, and failure results.
- [ ] Add restore actions and a recovery-history view.
- [ ] Ensure partial operations can be retried without duplicating successful moves.
- [ ] Test successful moves, missing files, locked files, destination conflicts, cancellation, restore conflicts, and repeated operations.

**Validation:**
- Run all move and restore tests exclusively against temporary directories.
- Manually perform a small cleanup, restart the application, and verify the operation remains discoverable before attempting restore.

### Task 10: Add persistence, scan profiles, and history

**Files:**
- Create: `src/core/persistence/SettingsStore.h` and `src/core/persistence/SettingsStore.cpp`
- Create: `src/core/persistence/ScanHistoryStore.h` and `src/core/persistence/ScanHistoryStore.cpp`
- Create: `src/core/persistence/OperationHistoryStore.h` and `src/core/persistence/OperationHistoryStore.cpp`
- Create: `src/core/persistence/ScanProfile.h`
- Create: `src/ui/dialogs/ScanConfigurationDialog.h` and `src/ui/dialogs/ScanConfigurationDialog.cpp`
- Create: `src/ui/views/HistoryView.h` and `src/ui/views/HistoryView.cpp`
- Create: `tests/unit/persistence_store_test.cpp`

**Interfaces:**
- Stores use platform-appropriate application data locations and safe-write behavior.
- `ScanProfile` contains roots, exclusions, patterns, symlink policy, and recommendation thresholds.
- History stores expose summaries and operation records without requiring the original scan result to remain in memory.

- [ ] Define the persisted schema for settings, profiles, scan summaries, exclusions, and operation records.
- [ ] Use atomic writes and handle corrupted or partially written data by preserving the last readable state.
- [ ] Add create, edit, duplicate, delete, and run actions for scan profiles.
- [ ] Mark cached scan information stale when paths or timestamps no longer match the filesystem.
- [ ] Add history filtering by date, path, result, and operation type.
- [ ] Test restart persistence, corrupted data, missing data directories, schema evolution, and duplicate operation identifiers.

**Validation:**
- Save profiles and history, close the application, reopen it, and verify all user-visible records survive.
- Simulate an interrupted write and confirm the application remains usable without silently inventing history.

### Task 11: Add change monitoring, reports, and user feedback

**Files:**
- Create: `src/core/platform/FilesystemMonitor.h` and `src/core/platform/FilesystemMonitor.cpp`
- Create: `src/core/reporting/ReportData.h`
- Create: `src/core/reporting/ReportExporter.h` and `src/core/reporting/ReportExporter.cpp`
- Create: `src/ui/views/ScanSummaryView.h` and `src/ui/views/ScanSummaryView.cpp`
- Create: `src/ui/dialogs/SettingsDialog.h` and `src/ui/dialogs/SettingsDialog.cpp`
- Create: `tests/unit/report_exporter_test.cpp`

**Interfaces:**
- `FilesystemMonitor` reports advisory path changes and never performs modifications.
- `ReportExporter` accepts stable report data and produces text or CSV output.
- `ScanSummaryView` presents totals, errors, largest consumers, duplicate savings, and recommended savings.

- [ ] Add monitoring for selected directories with explicit watcher limits and failure reporting.
- [ ] Offer targeted rescan for changed paths and mark untouched cached data as unchanged.
- [ ] Add scan-completion notifications and clear cancelled/failed/partial states.
- [ ] Add text and CSV reports for scans and cleanup operations.
- [ ] Add settings for monitoring, hashing, exclusions, retention, thresholds, themes, and notifications.
- [ ] Test watcher failure, changed/deleted paths, report escaping, Unicode paths, commas, newlines, and empty reports.

**Validation:**
- Modify files in a monitored fixture and verify DiskLens offers a rescan without changing files automatically.
- Open exported reports in a spreadsheet or text editor and verify paths and fields remain intact.

### Task 12: Complete accessibility, performance, and interaction polish

**Files:**
- Modify: `src/app/MainWindow.*`
- Modify: `src/ui/views/*`
- Modify: `src/ui/dialogs/*`
- Create: `tests/performance/scan_benchmark.cpp`
- Create: `docs/user-guide.md`
- Create: `docs/supported-behavior.md`

- [ ] Add accessible names, descriptions, keyboard focus order, and keyboard actions for the main workflow.
- [ ] Complete loading, empty, cancelled, partial, error, and stale-data states across all views.
- [ ] Add onboarding that explains scan scope, treemap interaction, duplicate selection, and recovery behavior.
- [ ] Add performance measurements for enumeration, aggregation, layout, hashing, model updates, and rendering.
- [ ] Batch model updates so large scans do not cause one UI refresh per file.
- [ ] Test deeply nested trees, many sibling entries, millions of fixture metadata rows where practical, long paths, Unicode, network-like latency, and low-memory behavior.
- [ ] Document safe usage, protected paths, recovery, supported behavior, and known limitations.

**Validation:**
- Run the application through the complete scan → inspect → duplicate/recommendation → preview → recover → restore workflow.
- Verify keyboard-only completion of the primary read-only analysis flow.
- Record benchmark results before and after any performance optimization.

### Task 13: Package and validate the Windows release

**Files:**
- Modify: `CMakeLists.txt`
- Create: `packaging/windows/` installer configuration and release metadata
- Create: `docs/release-windows.md`
- Create: `tests/integration/windows_platform_test.cpp` when Windows-specific coverage is required

- [ ] Add Windows application metadata, icon, version information, and install layout.
- [ ] Add native file-manager opening and document the Windows-specific behavior.
- [ ] Validate protected folders, locked files, removable drives, long paths, drive roots, and permission failures.
- [ ] Decide and document whether Windows cleanup uses the app-managed recovery area or a native Recycle Bin adapter for the release.
- [ ] Build an installer and verify install, upgrade, launch, scan, recover, restore, uninstall, and user-data retention behavior.
- [ ] Test a clean-machine installation with no development environment present.

**Validation:**
- Configure, build, and run the complete test suite in the selected Windows build configuration.
- Install the packaged application in an isolated environment and execute the documented smoke workflow.

### Task 14: Add macOS and Linux platform adapters

**Files:**
- Create: `src/core/platform/macos/`
- Create: `src/core/platform/linux/`
- Create: `packaging/macos/`
- Create: `packaging/linux/`
- Create: `docs/release-macos.md`
- Create: `docs/release-linux.md`

- [ ] Implement platform-specific application-data paths through the platform interface rather than conditional logic spread through the UI.
- [ ] Add native file-manager opening for Finder and the configured Linux file manager.
- [ ] Add permission guidance and platform-specific protected-location behavior.
- [ ] Add native Trash integration only after recovery semantics and conflict handling are proven on each platform.
- [ ] Produce the selected macOS bundle and Linux package formats and document their installation requirements.
- [ ] Validate scan, duplicate, recommendation, recovery, restore, report, and uninstall behavior on each supported platform.

**Validation:**
- Run shared unit tests on every supported platform.
- Run platform integration tests for permissions, mounted volumes, native file-manager opening, and recovery behavior.
- Execute the documented clean-install smoke workflow for each package format.

## Validation sequence for each task

For every task, follow this order:

1. Add or update deterministic tests for the behavior.
2. Run the narrowest relevant test first.
3. Run the complete test suite after the narrow test passes.
4. Launch the application when the task changes UI or lifecycle behavior.
5. Inspect partial failures, cancellation, and empty states—not only the happy path.
6. Record any new platform behavior or limitation in the documentation.

## Recommended implementation order

Complete Tasks 1–6 before building duplicate detection or cleanup. Complete Task 7 before Task 8 so duplicate recommendations can consume a tested duplicate service. Complete Task 9 only after the preview and recovery rules are agreed and tested. Tasks 10–12 improve the product after the core workflow works. Tasks 13–14 are release and portability work, not prerequisites for the first local Windows prototype.

## Definition of done for the first complete prototype

The first complete prototype is achieved when a user can select a folder, scan it without freezing the UI, inspect the tree/table/treemap, find exact duplicate files, receive explainable recommendations, preview selected actions, move files to recovery, restore them without overwrite, and see accurate results after restart. The prototype must pass unit and temporary-directory integration tests and must remain read-only until the user confirms an operation.

## Open questions

- Which Qt 6 patch version and compiler/toolchain are installed on the development machine?
- Which Linux packaging format should be supported first after Windows: AppImage, Flatpak, or a distribution package?
- Should the first Windows release use only the app-managed recovery area, or should native Recycle Bin integration be included once the adapter is tested?
