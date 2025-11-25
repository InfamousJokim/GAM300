#pragma once
#include "Core.h"
#include "AssetLoadContext.h"
#include "Assets.h"
#include <future>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>


#pragma warning(push)
#pragma warning(disable: 4251)


namespace Boom {

    /**
     * @brief Manages parallel asset loading using a thread pool
     *
     * This class implements a two-phase loading system:
     * 1. CPU Phase (parallel): Load file data, parse models/textures on worker threads
     * 2. GPU Phase (main thread): Upload parsed data to OpenGL
     */
    class BOOM_API AsyncAssetLoader {
    public:
        /**
         * @brief Create thread pool with specified number of workers
         * @param numThreads Number of worker threads (0 = hardware concurrency)
         */
        AsyncAssetLoader(size_t numThreads = 0);
        ~AsyncAssetLoader();

        /**
         * @brief Queue an asset for asynchronous loading
         * @param type Asset type
         * @param uid Unique asset ID
         * @param name Asset name
         * @param source File path
         * @param hasJoints For models: whether the model has skeletal joints
         */
        void QueueAsset(
            AssetType type,
            AssetID uid,
            const std::string& name,
            const std::string& source,
            bool hasJoints = false
        );

        /**
         * @brief Wait for all queued assets to finish loading (CPU phase)
         * @return Vector of loaded contexts ready for GPU upload
         */
        std::vector<AssetLoadContext> WaitForAll();

        /**
         * @brief Get number of assets currently being processed
         */
        size_t GetPendingCount() const;

        /**
         * @brief Check if all assets have finished loading
         */
        bool IsComplete() const;

        /**
         * @brief Get loading progress (0.0 to 1.0)
         */
        float GetProgress() const;

    private:
        void WorkerThread();
        void ProcessAsset(AssetLoadContext& context);

        std::vector<std::thread> m_Workers;
        std::queue<AssetLoadContext> m_TaskQueue;
        std::vector<AssetLoadContext> m_CompletedTasks;

        mutable std::mutex m_QueueMutex;
        mutable std::mutex m_CompletedMutex;
        std::condition_variable m_Condition;

        bool m_Shutdown = false;
        std::atomic<size_t> m_ActiveTasks{0};
        std::atomic<size_t> m_TotalTasks{0};
        std::atomic<size_t> m_CompletedCount{0};
    };

} // namespace Boom

#pragma warning(pop)
