#include <cmath>
#include <iostream>
#include <chrono>
#include <vector>
#include <array>
#include "thread_pool.hpp"

double function_with_return(uint64_t min, uint64_t max)
{
    double sum = 0.0;
    for (uint64_t i = min; i <= max; ++i)
    {
        sum += sqrt(i);
    }
    return sum;
}

auto function_with_return_2(int client_id)
    -> std::tuple<std::string, int, decltype(std::chrono::system_clock::now())>
{
    std::string response =
            "200 OK HTTP/1.1\r\n\
            \r\n\
            Hello, my client";
    return std::make_tuple(response, client_id, std::chrono::system_clock::now());
}

int main()
{
    uint64_t min = 0, max = 1e9;
    uint64_t threads = 12;
    uint64_t each_task = (max - min) / threads;

    // 线程池测试
    auto start = std::chrono::system_clock::now();

    // 计算密集型
    ThreadPool thread_pool;
    std::vector<std::future<double>> futures;
    for (uint64_t i = 0; i < threads; ++i)
    {
        uint64_t sub = each_task * i + min;
        uint64_t sup = each_task * (i + 1) - (i == 3 ? 0 : 1);
        futures.push_back(thread_pool.AddTask(function_with_return, sub, sup));
    }
    double sum = 0.0;
    for (uint64_t i = 0; i < threads; ++i)
    {
        sum += futures[i].get();
    }
    std::cout << "Result: " << sum << std::endl;

    auto end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double total_time = static_cast<double>(duration.count()) *
        std::chrono::microseconds::period::num /
        std::chrono::microseconds::period::den;
    std::cout << "Thread Pool: " << total_time << "s" << std::endl;

    // 服务密集型
    start = std::chrono::system_clock::now();
    std::vector<std::future<std::tuple<std::string, int, decltype(start)>>> futures_2;
    constexpr int client_cnt = 1e9;
    // 用于计算平均等待时间
    std::array<decltype(start), client_cnt> start_times, end_times;
    for (int i = 0; i < client_cnt; ++i)
    {
        start_times[i] = std::chrono::system_clock::now();
        futures_2.push_back(thread_pool.AddTask(function_with_return_2, i));
    }
    for (int i = 0; i < client_cnt; ++i)
    {
        auto [response, id, end_time] = futures_2[i].get();
        end_times[id] = end_time;
    }

    end = std::chrono::system_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    total_time = static_cast<double>(duration.count()) *
        std::chrono::microseconds::period::num /
        std::chrono::microseconds::period::den;
    std::cout << "Thread pool: " << total_time << "s" << std::endl;
    // 计算最大、最小等待时间和平均等待时间
    std::array<double, client_cnt> times;
    double total = 0.0, max_time = 0.0, min_time = 1e5;
    for (int i = 0; i < client_cnt; ++i)
    {
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end_times[i] - start_times[i]);
        times[i] = static_cast<double>(duration.count()) *
            std::chrono::microseconds::period::num /
            std::chrono::microseconds::period::den;
        total += times[i];
        if (times[i] > max_time)
        {
            max_time = times[i];
        }
        if (times[i] < min_time)
        {
            min_time = times[i];
        }
    }
    std::cout << "Max server time: " << max_time << "\n"
              << "Min server time: " << min_time << "\n"
              << "Average server time: " << total / static_cast<double>(client_cnt) << std::endl;


    // 多线程测试
    start = std::chrono::system_clock::now();

    std::vector<std::thread> threads_vec;
    std::vector<std::future<double>> futures_vec;
    for (uint64_t i = 0; i < threads; ++i)
    {
        uint64_t sub = each_task * i + min;
        uint64_t sup = each_task * (i + 1) - (i == 3 ? 0 : 1);
        futures_vec.push_back(std::async(function_with_return, sub, sup));
    }
    sum = 0.0;
    for (uint64_t i = 0; i < threads; ++i)
    {
        sum += futures_vec[i].get();
    }
    std::cout << "Result: " << sum << std::endl;

    end = std::chrono::system_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    total_time = static_cast<double>(duration.count()) *
        std::chrono::microseconds::period::num /
        std::chrono::microseconds::period::den;
    std::cout << "Multithread: " << total_time << "s" << std::endl;

    // 单线程测试
    start = std::chrono::system_clock::now();
    sum = function_with_return(min, max);
    std::cout << "Result: " << sum << std::endl;
    end = std::chrono::system_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    total_time = static_cast<double>(duration.count()) *
        std::chrono::microseconds::period::num /
        std::chrono::microseconds::period::den;
    std::cout << "Single thread: " << total_time << "s" << std::endl;
}