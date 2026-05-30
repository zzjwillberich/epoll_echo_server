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
    //线程池的构造函数
    ThreadsPool(int num){
        //创建线程的vector
        for(int i = 0;i < num;++i){
            threads.emplace_back([this](){
                worker();
            });
        }
    }

    //线程的工作函数
    void worker(){
        while(true){
            Task task;
            {
                //先上锁,保护队列
                std::unique_lock<std::mutex> ul(queue_lock);

                //等待先释放锁
                cv.wait(ul,[this](){
                    return stop || !task_queue.empty();
                });
                
                //拿到锁之后先看是要销毁还是有任务
                if(stop && task_queue.empty()){
                    break;
                }

                //有任务的情况,拿取任务
                task = std::move(task_queue.front());
                task_queue.pop();
            }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 
            task();
        }
    }


    //主进程将任务传给线程池的接口
    void submit(Task task){
        {
            std::lock_guard<std::mutex> lg(queue_lock);
            task_queue.push(task);
        }

        cv.notify_one();
    }


    //析构函数
    ~ThreadsPool(){
        {
            std::lock_guard<std::mutex> lg(queue_lock);
            stop = true;
        }
        cv.notify_all();

        for(auto& thread : threads){
            thread.join();
        }
    }
};
