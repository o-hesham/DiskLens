#pragma once

namespace DiskLens
{

    // Categorizes the problem that caused a file to be skipped.
    enum class DuplicateIssueKind
    {
        Missing,
        Unreadable,
        Changed,
        IdentityCheckFailed, // Its physical identity could not be established.
        Other
    };

} // namespace DiskLens
