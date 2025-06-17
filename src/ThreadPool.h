#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>

namespace qse {

class ThreadPool {
public:
    // Constructor creates the thread pool with specified number of threads
    explicit ThreadPool(size_t num_threads);
    
    // Destructor ensures all threads are joined
    ~ThreadPool();
    
    // Prevent copying of the thread pool
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    // Enqueue a task and return a future for its result
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;

private:
    // Vector of worker threads
    std::vector<std::thread> workers;
    
    // Queue of tasks
    std::queue<std::function<void()>> tasks;
    
    // Synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    
    // Flag to stop the thread pool
    bool stop;
};

// Template implementation of enqueue
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        
        // Don't allow enqueueing after stopping the pool
        if(stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        
        tasks.emplace([task](){ (*task)(); });
    }
    condition.notify_one();
    return res;
}

} // namespace qse 