#pragma once 
#include "core.h"
#include <string>
#include <functional>
#include <chrono>
#include <filesystem>
#include <unordered_map>

namespace Boom {


	class FileWatcher
	{
	public:
		using Callback = std::function<void(const std::string& filepath)>;
		using TimePoint = std::chrono::system_clock::time_point;

		FileWatcher() = default ;
		~FileWatcher() = default;

		// Watching the filepath Path to call when the scripting file is changing
		bool AddWatch(const std::string& filepath, Callback callback, int debounceMs = 500);

		// Remove a file from watching 
		void RemoveWatch(const std::string& filepath);

		// Check all watched files for changes
		void Update();

		// Clearing all of the watches 
		void ClearAll();

		bool IsWatching(const std::string& filepath) const;

	private:
		struct WatchEntry
		{
			std::string filepath;
			Callback callback;
			TimePoint lastModified;
			TimePoint lastTriggered;
			std::chrono::milliseconds debounceTime;
			bool pendingTrigger = false;
		};

		std::unordered_map<std::string, WatchEntry> m_Watches;

		static TimePoint GetFileWriteTime(const std::string& filepath);
	};

}