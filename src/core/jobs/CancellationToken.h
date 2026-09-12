#pragma once

#include <atomic>

namespace DiskLens
{

    // Allows a running task to safely check whether cancellation was requested.
    class CancellationToken final
    {
    public:
        // Request that the current task stop.
        void requestCancellation() noexcept
        {
            cancellationRequested_.store(true);
        }

        // Check whether cancellation has been requested.
        [[nodiscard]] bool isCancellationRequested() const noexcept
        {
            return cancellationRequested_.load();
        }

    private:
        // Atomic because it may be accessed from different threads.
        std::atomic_bool cancellationRequested_ = false;
    };

} // namespace DiskLens
