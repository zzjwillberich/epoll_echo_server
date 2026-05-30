#include "threadspool.h"

ThreadsPool::ThreadsPool(int num) : stop(false) {
    for (int i = 0; i < num; ++i) {
        threads.emplace_back([this]() {
            worker();
        });
    }
}

ThreadsPool::~ThreadsPool() {
    {
        std::lock_guard<std::mutex> lg(queue_lock);
        stop = true;
    }
    cv.notify_all();
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void ThreadsPool::submit(Task task) {
    {
        std::lock_guard<std::mutex> lg(queue_lock);
        task_queue.push(std::move(task));
    }
    cv.notify_one();
}

void ThreadsPool::worker() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> ul(queue_lock);
            cv.wait(ul, [this]() {
                return stop || !task_queue.empty();
            });
            if (stop && task_queue.empty()) {
                break;
            }
            task = std::move(task_queue.front());
            task_queue.pop();
        }
        task();
    }
}
