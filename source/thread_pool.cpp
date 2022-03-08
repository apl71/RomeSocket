#include "thread_pool.hpp"

ThreadPool::~ThreadPool()
{
    stop = true;
    condition.notify_all();
    for (auto &thread : threads)
    {
        thread.join();
    }
}

ThreadPool::ThreadPool(size_t min_threads, size_t max_threads)
{
    stop = false;
    if (min_threads > max_threads)
    {
        min_threads = max_threads = std::thread::hardware_concurrency();
    }
    min_thread_cnt = min_threads;
    max_thread_cnt = max_threads;
    threads.reserve(min_threads);
    for (size_t i = 0; i < min_threads; ++i)
    {
        threads.push_back(std::thread(
            [this]()
            {
                while (true)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->mutex);
                        /*
                            第二个参数的作用是，当线程池停止或者还有任务可做时，
                            不要在原地等待，而是继续执行，使得线程退出或者执行
                            任务，如果去掉，线程池不会自动取出任务，因为notify
                            时，线程很有可能正在执行代码，而非wait，导致任务被
                            搁置，无法执行。 
                        */
                        this->condition.wait(lock,
                            [this]()
                            {
                                return this->stop || !this->tasks.empty();
                            });
                        if (this->stop && this->tasks.empty())
                        {
                            return;
                        }
                        task = std::move(this->tasks.back());
                        tasks.pop_back();
                    }
                    task();
                }
            }));
    }
}