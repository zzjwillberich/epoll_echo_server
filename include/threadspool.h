#ifndef THEADSPOOL_H
#define THREADPOOL_H

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadsPool{
    using Task = std::function<void()>;
    std::queue<Task> task_queue;
    std::mutex queue_lock;
    std::condition_variable cv;
    bool stop;
    std::vector<std::thread> threads;

    public:
    ThreadsPool(int num){}

    void worker(){}

    void submit(Task task){}

    ~ThreadsPool(){}
};
#endif