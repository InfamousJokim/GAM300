#include "Core.h"
#pragma warning(disable : 4834) // Disable [[nodiscard]] warnings for exception::what() in logging
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

        // Create a list of paths to process (avoid issues with corrupted strings during iteration)
        std::vector<std::string> pathsToCheck;
        pathsToCheck.reserve(m_Watches.size());

        for (const auto& [path, entry] : m_Watches) {
            try {
                // Validate the path string before using it
                if (path.empty()) {
                    BOOM_WARN("[FileWatcher] Empty path found in watches, skipping");
                    continue;
                }
                pathsToCheck.push_back(path);
            }
            catch (...) {
                BOOM_ERROR("[FileWatcher] Corrupted path found in watches, skipping");
                continue;
            }
        }

        // Now process each path safely
        for (const auto& path : pathsToCheck) {
            auto it = m_Watches.find(path);
            if (it == m_Watches.end()) continue;

            auto& entry = it->second;

            try {
                // Check if file exists
                if (!std::filesystem::exists(path)) {
                    continue; // File was deleted, skip this frame
                }

                TimePoint currentWriteTime = GetFileWriteTime(path);

                // Check if file was modified
                if (currentWriteTime != entry.lastModified) {
                    entry.lastModified = currentWriteTime;
                    entry.pendingTrigger = true;
                    entry.lastTriggered = now;
                }

                // Trigger callback after debounce period
                if (entry.pendingTrigger) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - entry.lastTriggered
                    );
                    if (elapsed >= entry.debounceTime) {
                        BOOM_INFO("[FileWatcher] Triggering callback for: {}", path);
                        entry.callback(path);
                        entry.pendingTrigger = false;
                    }
                }
            }
            catch (const std::filesystem::filesystem_error& e) {
                BOOM_ERROR("[FileWatcher] Filesystem error for {}: {}", path, e.what());
                continue;
            }
            catch (const std::exception& e) {
                BOOM_ERROR("[FileWatcher] Error processing {}: {}", path, e.what());
                continue;
            }
            catch (...) {
                BOOM_ERROR("[FileWatcher] Unknown error processing file");
                continue;
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
            const char* error_msg = e.what();
            BOOM_ERROR("[FileWatcher] Failed to get write time for {}: {}",
                filepath, error_msg);
            return std::chrono::system_clock::now();
        }
    }

}