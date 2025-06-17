#include "ThreadPool.h"

namespace qse {

// The constructor creates the worker threads.
ThreadPool::ThreadPool(size_t num_threads) {
    for(size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] {
            // This is the main loop for each worker thread.
            for(;;) {
                std::function<void()> task;

                {
                    // Acquire a unique lock on the queue mutex.
                    // The lock is released automatically when it goes out of scope.
                    std::unique_lock<std::mutex> lock(this->queue_mutex_);

                    // Wait on the condition variable. The thread will sleep until:
                    // 1. The pool is stopped.
                    // 2. The tasks queue is not empty.
                    this->condition_.wait(lock, [this] {
                        return this->stop_ || !this->tasks_.empty();
                    });

                    // If the pool is stopped AND the queue is empty, the thread can exit.
                    if(this->stop_ && this->tasks_.empty()) {
                        return;
                    }

                    // Pop the next task from the queue.
                    task = std::move(this->tasks_.front());
                    this->tasks_.pop();
                } // The lock is released here.

                // Execute the task outside of the lock.
                task();
            }
        });
    }
}

// The destructor makes sure all threads are properly shut down.
ThreadPool::~ThreadPool() {
    {
        // Acquire lock to safely modify the stop flag.
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }

    // Wake up all waiting threads so they can check the stop flag and exit.
    condition_.notify_all();

    // Wait for each worker thread to complete its execution.
    for(std::thread &worker: workers_) {
        worker.join();
    }
}

} // namespace qse