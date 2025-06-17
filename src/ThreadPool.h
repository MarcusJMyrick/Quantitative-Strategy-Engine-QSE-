#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <type_traits> // For std::invoke_result_t, the modern replacement for std::result_of

namespace qse {

class ThreadPool {
public:
    // The constructor launches the worker threads.
    ThreadPool(size_t num_threads);

    // The destructor stops and joins all threads.
    ~ThreadPool();

    // Enqueues a task (function) to be executed by a worker thread.
    // It returns a std::future, which allows the caller to wait for the task's result.
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

private:
    // A vector to hold the worker threads.
    std::vector<std::thread> workers_;

    // The queue of tasks to be executed.
    std::queue<std::function<void()>> tasks_;

    // Synchronization primitives to ensure thread safety.
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_ = false; // Flag to signal threads to stop.
};

// --- Template Implementation ---
// Template member function definitions must be in the header file
// so the compiler can see them when they are instantiated in other files.

template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>> {

    // Use std::invoke_result_t to determine the function's return type.
    using return_type = std::invoke_result_t<F, Args...>;

    // A packaged_task wraps the function and its return value into a future.
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    // Get the future from the packaged_task before moving it.
    std::future<return_type> res = task->get_future();

    {
        // Lock the queue to safely add the new task.
        std::unique_lock<std::mutex> lock(queue_mutex_);

        // Don't allow enqueueing new tasks after the pool has been stopped.
        if(stop_) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        // Add the task to the queue.
        // We wrap it in a lambda to erase its specific type, storing it as a std::function<void()>.
        tasks_.emplace([task](){ (*task)(); });
    }

    // Notify one waiting worker thread that a new task is available.
    condition_.notify_one();
    return res;
}

} // namespace qse