#include "Core.h"
#include "Scripting/FileWatcher.h"

namespace Boom {

    bool FileWatcher::AddWatch(const std::string& filepath,
        Callback callback,
        int debounceMs)
    {
        if (!std::filesystem::exists(filepath)) {
            BOOM_ERROR("[FileWatcher] File does not exist: {}", filepath);
            return false;
        }

        WatchEntry entry;
        entry.filepath = filepath;
        entry.callback = callback;
        entry.lastModified = GetFileWriteTime(filepath);
        entry.lastTriggered = std::chrono::system_clock::now();
        entry.debounceTime = std::chrono::milliseconds(debounceMs);
        entry.pendingTrigger = false;

        m_Watches[filepath] = entry;

        BOOM_INFO("[FileWatcher] Now watching: {}", filepath);
        return true;
    }

    void FileWatcher::RemoveWatch(const std::string& filepath)
    {
        auto it = m_Watches.find(filepath);
        if (it != m_Watches.end()) {
            BOOM_INFO("[FileWatcher] Stopped watching: {}", filepath);
            m_Watches.erase(it);
        }
    }

    void FileWatcher::Update()
    {
        auto now = std::chrono::system_clock::now();

        for (auto& [path, entry] : m_Watches) {
            if (!std::filesystem::exists(entry.filepath)) {
                continue; // File was deleted, skip this frame
            }

            TimePoint currentWriteTime = GetFileWriteTime(entry.filepath);

            // Check if file was modified
            if (currentWriteTime != entry.lastModified) {
                entry.lastModified = currentWriteTime;
                entry.pendingTrigger = true;
                entry.lastTriggered = now;

                //BOOM_INFO("[FileWatcher] Detected change: {}", entry.filepath);
            }

            // Trigger callback after debounce period
            if (entry.pendingTrigger) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - entry.lastTriggered
                );

                if (elapsed >= entry.debounceTime) {
                    BOOM_INFO("[FileWatcher] Triggering callback for: {}", entry.filepath);
                    entry.callback(entry.filepath);
                    entry.pendingTrigger = false;
                }
            }
        }
    }

    void FileWatcher::ClearAll()
    {
        BOOM_INFO("[FileWatcher] Clearing all watches");
        m_Watches.clear();
    }

    bool FileWatcher::IsWatching(const std::string& filepath) const
    {
        return m_Watches.find(filepath) != m_Watches.end();
    }

    FileWatcher::TimePoint FileWatcher::GetFileWriteTime(const std::string& filepath)
    {
        try {
            auto ftime = std::filesystem::last_write_time(filepath);
            // Convert file_time to system_clock time
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - std::filesystem::file_time_type::clock::now() +
                std::chrono::system_clock::now()
            );
            return sctp;
        }
        catch (const std::filesystem::filesystem_error& e) {
            BOOM_ERROR("[FileWatcher] Failed to get write time for {}: {}",
                filepath, e.what());
            return std::chrono::system_clock::now();
        }
    }

}