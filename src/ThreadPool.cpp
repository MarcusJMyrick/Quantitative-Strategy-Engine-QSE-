#include "ThreadPool.h"

namespace qse {

ThreadPool::ThreadPool(size_t num_threads) : stop(false) {
    for(size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([this] {
            while(true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    
                    // Wait until there is a task or the pool is stopped
                    this->condition.wait(lock, [this] {
                        return this->stop || !this->tasks.empty();
                    });
                    
                    // If stopped and no tasks, exit
                    if(this->stop && this->tasks.empty()) {
                        return;
                    }
                    
                    // Get the next task
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                
                // Execute the task
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    
    // Notify all threads to wake up and check the stop condition
    condition.notify_all();
    
    // Join all threads
    for(std::thread &worker: workers) {
        if(worker.joinable()) {
            worker.join();
        }
    }
}

} // namespace qse 