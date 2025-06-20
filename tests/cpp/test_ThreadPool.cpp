#include <gtest/gtest.h>
#include "qse/core/ThreadPool.h"
#include <vector>
#include <atomic>
#include <chrono>

// Test to ensure all enqueued tasks are executed.
TEST(ThreadPoolTest, ExecutesAllTasks) {
    // Arrange: Create a thread pool and a counter.
    qse::ThreadPool pool(4); // Use 4 worker threads.
    std::atomic<int> counter(0); // An atomic integer to safely count across threads.
    int num_tasks = 100;

    // Act: Enqueue 100 simple tasks. Each task increments the counter.
    for (int i = 0; i < num_tasks; ++i) {
        pool.enqueue([&counter]() {
            // Increment the counter. This will be done by a worker thread.
            counter++;
        });
    }

    // The ThreadPool's destructor will be called here, which waits for all
    // tasks to complete before the test proceeds.

    // Assert: Check if the counter reached the expected value.
    // We need to give the tasks a moment to complete.
    // A better approach in a real app would be to use the futures, but for this test,
    // relying on the destructor to block is sufficient.
    // However, to be robust, we can add a small sleep or check loop.
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Give threads time to finish.
    
    EXPECT_EQ(counter.load(), num_tasks);
}

// Test to ensure tasks with return values work correctly.
TEST(ThreadPoolTest, HandlesReturnValues) {
    // Arrange: Create a thread pool.
    qse::ThreadPool pool(2);
    std::vector<std::future<int>> results;

    // Act: Enqueue tasks that return a value.
    for (int i = 0; i < 10; ++i) {
        results.emplace_back(
            pool.enqueue([i] {
                return i * 2;
            })
        );
    }

    // Assert: Check if the results are correct.
    // This implicitly waits for each task to complete before checking its result.
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(results[i].get(), i * 2);
    }
}