#ifndef THREADSPOOL_H
#define THREADSPOOL_H

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadsPool {
    using Task = std::function<void()>;
    std::queue<Task> task_queue;
    std::mutex queue_lock;
    std::condition_variable cv;
    bool stop = false;
    std::vector<std::thread> threads;

public:
    explicit ThreadsPool(int num);
    ~ThreadsPool();

    void submit(Task task);

private:
    void worker();
};

#endif
