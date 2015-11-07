# ThreadPool
Lightweight, Generic, Pure C++11 ThreadPool

## Rational
I needed a Thread Pool for something I was writing, and I didn't see any that I liked. This is still somewhat a work in progress; it's stable, but there are definitely places where some of the locking logic could be better.

## Licensing 
Unlicensed, I don't want recognition. Use at your own risk for whatever you want. 

## Overview
ThreadPool is a super simple class that manages threads and jobs. Jobs are functions with no parameters or return values. This decision was to make it as generic as possible so it could be integrated into a variety of projects.

```c++
template <unsigned ThreadCount = 10>
class ThreadPool {
public:
    ThreadPool();
    ~ThreadPool();
    void AddJob( std::function<void(void)> );
    unsigned Size();
    void JoinAll( bool WaitForAll = true );
    void WaitAll();
};
```

## Example
```c++
#include <ThreadPool.h>

int main() {
    using nbsdx::concurrent::ThreadPool;
    
    ThreadPool pool;
    int JOB_COUNT = 100;
    
    for( int i = 0; i < JOB_COUNT; ++i )
        pool.AddJob( []() { 
            std::this_thread::sleep_for( std::chrono_seconds( 1 ) );
        } );
    
    pool.JoinAll();
    std::cout << "Expected runtime: " << (JOB_COUNT / pool.Size()) 
              << " seconds." << std::endl;
}
```
