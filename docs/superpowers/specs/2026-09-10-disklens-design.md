# DiskLens Design Specification

## Product summary

DiskLens is a cross-platform desktop application for understanding and safely reclaiming disk space. Its primary experience is an interactive treemap that exposes large files and folders, supported by duplicate detection, cleanup recommendations, previews, and recoverable file operations.

The application will be built with Qt 6 and CMake. The core will remain platform-independent; Windows will be the first tested and packaged platform, followed by macOS and Linux integrations where the operating systems differ.

## Goals

- Help users understand what occupies storage before taking action.
- Find exact duplicate files reliably.
- Recommend cleanup candidates without making unexplained decisions.
- Make cleanup reversible by default.
- Keep scanning responsive for large directory trees.
- Provide a polished Qt desktop application suitable for portfolio release.
- Preserve a clean separation between filesystem logic, analysis, persistence, and UI.

## Non-goals for the first release

- Secure file erasure or forensic destruction.
- Registry cleaning, driver cleaning, or operating-system repair.
- Cloud-storage administration.
- Automatic deletion without explicit user confirmation.
- Disk-health diagnostics such as SMART monitoring.
- Image or video semantic similarity in the initial duplicate finder.

## Product scope

### Scanning

Users can choose one or more folders or mounted volumes to scan. The scanner recursively discovers files and directories, collects metadata, reports inaccessible paths, supports cancellation, and emits progress without blocking the UI.

The scanner must detect symlinks and avoid following them by default. It must prevent recursive loops, tolerate files disappearing during a scan, and distinguish permission failures from ordinary empty directories. A scan can be restricted by include/exclude rules and file-name patterns.

### Storage analysis

DiskLens maintains a hierarchical model of volumes, directories, and files. The model supports total logical size, file counts, file-type totals, timestamps, and scan errors. It powers both the visual treemap and conventional table/tree views.

The main analysis view contains:

- Interactive treemap with folder/file rectangles sized by logical bytes.
- Breadcrumb navigation and zoom into a selected directory.
- Hover and selection details.
- Color grouping by file type or directory.
- Sortable file table.
- Search and filters for size, extension, age, path, and duplicate status.
- Actions to open a path in the native file manager or copy its path.

### Duplicate detection

Duplicate detection uses staged comparison:

1. Group files by compatible size.
2. Compare a small content sample when useful to reject obvious non-matches.
3. Compute a complete content hash for remaining candidates.
4. Group files with identical complete hashes.

The UI shows duplicate groups, file paths, sizes, dates, and previews where practical. Users can select individual files or use explicit policies such as keep newest, keep oldest, or keep the shortest path. The policy result must remain editable before cleanup.

### Cleanup recommendations

DiskLens provides explainable recommendations for:

- Very large files.
- Files not modified for a user-selected period.
- Temporary files.
- Recognized application caches.
- Old downloads.
- Empty directories.
- Exact duplicates.

Recommendations are informational until the user selects an action. Each item explains the rule, lists the affected paths, estimates recoverable space, and identifies whether the action is reversible.

Cleanup rules must be conservative. System-protected locations, application data with unclear ownership, recently modified files, and files outside the selected scan scope are excluded unless the user explicitly changes the relevant setting.

### Safe file operations

The initial cross-platform recovery mechanism is an app-managed recovery area. Before moving a file, DiskLens records its original path, destination path, size, timestamp, hash when available, and operation identifier. The operation is shown as a preview and requires confirmation.

Users can restore a completed move when the original path is available and no conflicting file exists. Conflicts must be reported instead of overwritten. Failed multi-file operations must show per-file results and must not claim complete success.

Permanent deletion is not part of the default workflow. Native Recycle Bin/Trash integration may be added as a platform adapter after the core recovery flow is reliable.

### Persistence and monitoring

DiskLens stores settings, scan locations, exclusions, scan profiles, scan history, and cleanup-operation history in the platform-appropriate application data location. Scan results may be cached, but cached entries must be marked stale when the filesystem changes.

The application can monitor selected paths for changes and offer a targeted rescan. Monitoring is advisory; it must not silently modify files.

### Reporting and usability

- Scan completion summary with total size, file count, errors, and largest consumers.
- Exportable text or CSV report for a scan and cleanup operation.
- Dark and light themes.
- Keyboard-accessible navigation and actions.
- Clear empty, loading, cancelled, and error states.
- Notifications when long-running scans finish.
- Settings for scan behavior, exclusions, hashing, retention, and appearance.

## Architecture

The application is divided into independently testable modules:

1. **Platform layer** — path conventions, mounted-volume discovery, native file-manager opening, and future Trash/Recycle Bin adapters.
2. **Filesystem layer** — directory enumeration, metadata reads, permission classification, symlink policy, and safe move/restore operations.
3. **Analysis layer** — hierarchy construction, size aggregation, file-type classification, staged hashing, duplicate grouping, and recommendation rules.
4. **Persistence layer** — settings, scan history, cached metadata, exclusions, and operation records.
5. **Application services** — scan jobs, hash jobs, cleanup jobs, cancellation, progress, and error aggregation.
6. **Presentation layer** — Qt models, views, treemap rendering, dialogs, commands, and user feedback.

The UI must communicate with background work through explicit job/service interfaces. Filesystem and hashing work must never run synchronously on the GUI thread. UI models consume analysis results rather than performing filesystem traversal themselves.

## Data flow

`User selects scope → scan service enumerates entries → analysis model aggregates hierarchy → UI renders tree/table/treemap → user requests duplicate analysis or recommendation → background service computes result → UI previews operation → recovery service moves/restores files → persistence records outcome`.

Every long-running operation has a queued, running, cancelling, completed, cancelled, or failed state. Progress should include meaningful counts and byte totals where available, while allowing unknown totals for filesystems that cannot provide them cheaply.

## Safety requirements

- Default to read-only analysis.
- Never follow symlinks by default.
- Do not overwrite during move or restore.
- Confirm the exact number of files and estimated bytes before cleanup.
- Preserve original paths and operation metadata.
- Handle files changing, disappearing, becoming locked, or losing permissions during work.
- Treat file names and paths as untrusted input for display and export.
- Prevent recovery-path collisions and path traversal when reconstructing destinations.
- Keep cleanup actions scoped to the user-selected scan scope.
- Make cancellation cooperative and report partial results honestly.

## Milestones and acceptance criteria

### Milestone 0 — Product and safety rules

Define supported initial platforms, protected-path policy, duplicate semantics, recovery behavior, cleanup recommendation rules, and the initial window layout.

Acceptance: the rules are written down, internally consistent, and used as the source of truth for later work.

### Milestone 1 — Qt application foundation

Set up the Qt/CMake application, main window, menus, toolbar, status bar, settings boundary, logging, theme switching, and placeholder navigation.

Acceptance: the application launches in a clean state, displays its version, saves settings, and exits without errors on the development platform.

### Milestone 2 — Filesystem scanner

Implement folder selection, recursive enumeration, metadata collection, progress, cancellation, exclusions, symlink policy, and structured scan errors.

Acceptance: scans complete without freezing the UI; cancellation works; inaccessible, deleted, and changing files are reported accurately.

### Milestone 3 — Filesystem and analysis models

Implement models for entries, folders, volumes, file types, scan summaries, and errors. Add size aggregation and stable identifiers for entries.

Acceptance: model-level tests verify hierarchy construction, totals, filtering inputs, and error classification without requiring a real large drive.

### Milestone 4 — Tree, table, search, and details views

Add the conventional browsing interface, sorting, filters, search, details, native file-manager opening, and path-copy actions.

Acceptance: a user can locate any scanned file through the tree, table, search, or details panel and can inspect its metadata.

### Milestone 5 — Interactive treemap

Implement a performant treemap view with navigation, selection, tooltips, color modes, breadcrumbs, and synchronization with the tree/table selection.

Acceptance: treemap rectangles represent the same hierarchy and totals as the data model, and navigation remains usable with large result sets.

### Milestone 6 — Duplicate finder

Implement size candidate grouping, staged content comparison, complete hashing, duplicate groups, previews, selection policies, and duplicate filters.

Acceptance: known duplicate and non-duplicate fixtures produce correct groups; files with equal names or sizes but different content are not reported as duplicates.

### Milestone 7 — Recovery and cleanup operations

Implement preview, app-managed recovery, move, restore, conflict detection, cancellation, per-file results, and operation history.

Acceptance: selected files can be moved and restored without silent overwrite; simulated failures are visible; repeated operations are safe and traceable.

### Milestone 8 — Explainable cleanup recommendations

Add rules for large, old, temporary, cache, download, empty-directory, and duplicate candidates. Add explanations, estimated reclaimable space, protected paths, and configurable thresholds.

Acceptance: each recommendation identifies its rule and affected paths, and no recommendation performs an action by itself.

### Milestone 9 — Persistence and targeted monitoring

Add saved scan locations, profiles, exclusions, scan history, stale-result handling, filesystem change notifications, and targeted rescans.

Acceptance: settings and history survive restart; changed paths can be rescanned without requiring a full unrelated scan.

### Milestone 10 — Reports, accessibility, and polish

Add scan and operation reports, keyboard navigation, accessible labels, loading/empty/error states, notifications, onboarding, and performance instrumentation.

Acceptance: a new user can complete a scan and understand the result without external instructions; keyboard navigation covers the main workflow.

### Milestone 11 — Windows release

Add Windows-specific file-manager and recovery integration where justified, protected-folder handling, installer packaging, upgrade behavior, and Windows validation.

Acceptance: a clean Windows installation can scan normal and protected locations, perform recoverable cleanup, and uninstall without leaving undocumented user data.

### Milestone 12 — macOS and Linux support

Add platform adapters, packaging, permission guidance, native Trash integration where reliable, file-manager opening, standard data locations, and platform-specific cleanup rules.

Acceptance: the shared core passes its tests and each supported platform has a documented install, scan, cleanup, restore, and uninstall path.

## Later enhancements

After the first release is stable, consider similar-image detection, duplicate videos, directory comparison, scan snapshots, “changed since last scan,” portable mode, plugin-based cleanup rules, command-line scanning, localization, scheduled scans, and richer file previews.

## Testing strategy

- Unit-test path normalization, symlink policy, metadata aggregation, filters, hashing stages, duplicate grouping, recommendation rules, recovery naming, and conflict detection.
- Use temporary test directories with generated fixtures; never test destructive behavior against user directories.
- Test cancellation and partial failures explicitly.
- Test model/view behavior with deterministic fixture data.
- Use performance fixtures containing many files and deeply nested directories.
- Run platform-specific tests for permissions, locked files, removable volumes, native file-manager opening, and recovery behavior.
- Verify release packages on clean machines or isolated environments.

## Initial scope boundary

The first executable product should stop after Milestone 7 if necessary: scanning, visualization, duplicate detection, and safe recovery form a complete product. Recommendations, monitoring, packaging refinements, and advanced detection can follow without changing the core data model.

## Open decisions for implementation planning

- The exact Qt 6 patch version and compiler/toolchain will be selected from the development environment during project setup.
- The first release will use an app-managed recovery area; native Trash/Recycle Bin support is an adapter milestone, not a prerequisite for core cleanup.
- The first duplicate finder will identify exact content duplicates; perceptual similarity is explicitly deferred.
