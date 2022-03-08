#ifndef THREAD_POOL_HPP_
#define THREAD_POOL_HPP_

#include <vector>
#include <thread>
#include <list>
#include <future>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <atomic>
#include <memory>

class ThreadPool
{
private:
    size_t min_thread_cnt, max_thread_cnt;
    std::vector<std::thread> threads;
    std::list<std::function<void()>> tasks;
    std::condition_variable condition;
    std::mutex mutex;
    std::atomic<bool> stop;

    std::mutex io_mutex; // debug
public:
    ThreadPool() : ThreadPool(
                    static_cast<size_t>(std::thread::hardware_concurrency()),
                    static_cast<size_t>(std::thread::hardware_concurrency())) {}
    ThreadPool(size_t min_threads, size_t max_threads);
    ~ThreadPool();

    template <typename F, typename ... Args>
    auto AddTask(F &&func, Args&& ... args)
        -> std::future<decltype(func(args ...))>;
};

template <typename F, typename ... Args>
auto ThreadPool::AddTask(F &&func, Args&& ... args)
    -> std::future<decltype(func(args ...))>
{
    using ReturnType = decltype(func(args ...));
    auto new_task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(func), std::forward<Args>(args) ...));
    {
        std::unique_lock<std::mutex> lock(mutex);
        tasks.push_front([new_task](){(*new_task)();});
    }
    condition.notify_one();
    return new_task->get_future();
}

#endif