#ifndef THREAD_POOL
#define THREAD_POOL

#include <thread>
#include <vector>

static __thread int thread_num;

typedef void (*Task) ();

template<int Threads, typename T = Task>
class ThreadPool {
    public:
        ThreadPool(T task);

        void join();

    private:
        std::vector<std::thread> threads;
};

template<int Threads, typename T>
ThreadPool<Threads, T>::ThreadPool(T task){
    for(int i = 0; i < Threads; ++i){
       threads.push_back(std::thread([=](){
            thread_num = i;
            task();    
       })); 
    }
}

template<int Threads, typename T>
void ThreadPool<Threads, T>::join(){
    for(int i = 0; i < Threads; ++i){
        threads[i].join();
    }
}

#endif
