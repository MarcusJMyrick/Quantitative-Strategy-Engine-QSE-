#include <gtest/gtest.h>
#include "ThreadPool.h"
#include <atomic>
#include <vector>
#include <future>

namespace qse {
namespace test {

class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a thread pool with 4 threads
        pool = std::make_unique<ThreadPool>(4);
    }

    void TearDown() override {
        // ThreadPool destructor will clean up
        pool.reset();
    }

    std::unique_ptr<ThreadPool> pool;
};

TEST_F(ThreadPoolTest, ExecutesAllTasks) {
    std::atomic<int> counter(0);
    std::vector<std::future<void>> results;
    
    // Enqueue 100 tasks that each increment the counter
    for(int i = 0; i < 100; ++i) {
        results.push_back(
            pool->enqueue([&counter]() {
                counter++;
            })
        );
    }
    
    // Wait for all tasks to complete
    for(auto& result : results) {
        result.wait();
    }
    
    EXPECT_EQ(counter.load(), 100);
}

TEST_F(ThreadPoolTest, ReturnsCorrectResults) {
    std::vector<std::future<int>> results;
    
    // Enqueue tasks that return values
    for(int i = 0; i < 10; ++i) {
        results.push_back(
            pool->enqueue([i]() {
                return i * i;
            })
        );
    }
    
    // Check that each task returned the correct result
    for(int i = 0; i < 10; ++i) {
        EXPECT_EQ(results[i].get(), i * i);
    }
}

TEST_F(ThreadPoolTest, HandlesExceptions) {
    auto future = pool->enqueue([]() {
        throw std::runtime_error("Test exception");
    });
    
    EXPECT_THROW(future.get(), std::runtime_error);
}

} // namespace test
} // namespace qse 