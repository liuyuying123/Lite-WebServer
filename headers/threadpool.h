
#ifndef MY_WEBSERVER_THREADPOOL_H
#define MY_WEBSERVER_THREADPOOL_H

#include<mutex>
#include<queue>
#include<thread>
#include<functional>
#include<condition_variable>
#include<assert.h>

class ThreadPool {
public:
    /*
     * 构造函数中根据传入的参数构建线程池
     * 线程是调用detach
     */
    explicit ThreadPool(size_t threadCount = 8,int max_tasks=1000) : pool_(std::make_shared<Pool>(max_tasks)) {
        assert(threadCount > 0);

        for (size_t i = 0; i < threadCount; i++) {
            /*
            使用lambda表达式构建执行对象
            将pool_传递给匿名函数，让匿名函数能够在函数体当中使用pool_
            */
            std::thread([pool = pool_] {
                /*初始化一个unique lock后面通过此对象的lock与unlock方法进行加锁与解锁，这样比每次都初始化一个unique lock对象要省资源*/
                std::unique_lock<std::mutex> locker(pool->mtx); //加锁

                while (true) {

                    if (!pool->tasks.empty()) {
                        /*任务队列不为空，进入本段代码，采用右值的方式取出任务，任务取出成功，解锁，执行完任务重新获取锁*/
                        auto task = std::move(pool->tasks.front());
                        pool->tasks.pop();
                        locker.unlock();
                        /*执行任务*/
                        task();
                        locker.lock(); //执行完毕，又想要获取锁来取出新任务
                    } else if (pool->isClosed) {
                        /*说明线程池收到了关闭信号，直接跳出循环*/
                        break;
                    } else {
                        /*到了此分支，说明任务队列为空，此时条件变量等待，wait方法会自动释放锁（这样其他的线程也可以来请求获取锁），当收到notify信号时重新尝试获取锁*/
                        pool->cond.wait(locker);
                    }
                }
            }).detach();  /*detach的形式运行线程*/
        }
    }

    /*
     * 让编译器生成一个默认的移动构造函数
     */
    ThreadPool(ThreadPool &&) = default;

    /*
     * 析构函数中将pool_->isClosed = true;标志置为false，这样detach的线程会自动关闭
     */
    ~ThreadPool(){
        if(static_cast<bool>(pool_)){
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }
            /*唤醒所有线程，这个线程池的逻辑是执行完了任务队列中的所有任务后才会退出线程*/
            pool_->cond.notify_all();
        }
    }

    /*
     * 类成员模板函数，参数自动推断，向任务队列中添加任务
     * 这里可以设置一个最大任务数量，若超过此数量，禁止向队列加入任务
     */
    template<class F>
    bool addTask(F &&task){
        /*利用RAII自动加锁解锁下面这块作用域*/
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            /*判断目前任务队列中任务的数量，如果大于最大数量，下一轮再添加*/
            if(pool_->tasks.size()>pool_->max_tasks){
                return false;
            }
            /*完美转发,将参数task原封不动地转发给emplace函数*/
            pool_->tasks.emplace(std::forward<F>(task));
        }
        /*加入一个任务，唤醒一个线程*/
        pool_->cond.notify_one();
        return true;
    }

private:
    /*定义一个结构体，保存相关变量*/
    struct Pool {
        Pool(int max_task):max_tasks(max_task){}
        std::mutex mtx;  /*互斥量*/
        std::condition_variable cond;  /*条件变量*/
        bool isClosed;  /*标志变量，表示是否关闭线程池*/
        std::queue<std::function<void()>> tasks;  /*任务队列*/
        int max_tasks; /*最大任务数量*/
    };

    std::shared_ptr<Pool> pool_;  /*因为线程是在detach模式下运行的，所以这里使用动态申请的堆内存空间，使用shareptr管理*/
};

#endif //MY_WEBSERVER_THREADPOOL_H
