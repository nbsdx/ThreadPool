# ThreadPool
Lightweight, Generic, Pure C++11 ThreadPool

## Rational
I needed a Thread Pool for something I was writing, and I didn't see any that I liked. This is still somewhat a work in progress; it's stable, but there are probably places where some of the locking logic could be better. `ThreadPool::JoinAll` is a little sloppy, but it works. 

## Licensing 
Public Domain. If my licensing is wrong, please let me know. Use at your own risk for whatever you want. Feel free to change the namespaces as well. Apparently licensing is hard and complicated. If your country doesn't have a public domain, feel free to say you found this on the side of the road. 

## Overview
`ThreadPool` is a super simple class that manages threads and jobs. `threadCount` threads are created at object instantiation time, and persist until the `ThreadPool` object is destroyed. You cannot change the thread count. Jobs are functions with no parameters or return values. This decision was to make it as generic as possible so it could be integrated into a variety of projects. If you can't get your job to work with those constraints, you're doing something wrong, or you need to roll your own ThreadPool. But you're probably making things overly complicated.

Below is a quick overview, but ThreadPool.h is documented, so just read that. It's less than 200 lines with comments.

```c++
class ThreadPool {
public:
    ThreadPool( unsigned threadCount = std::thread::hardware_concurrency() );
    ~ThreadPool();
    void AddJob( std::function<void(void)> );
    unsigned Size() const;
    unsigned JobsRemaining();
    void JoinAll( bool WaitForAll = true );
    void WaitAll();
};
```

## Examples
```c++
#include "ThreadPool.h"

#include <iostream>
#include <chrono>

int main() {
    using nbsdx::concurrent::ThreadPool;
    
    // 10 threads for 100 jobs
    ThreadPool pool(10);
    int JOB_COUNT = 100;
    
    for( int i = 0; i < JOB_COUNT; ++i )
        pool.AddJob( []() { 
            std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
        } );
    
    pool.JoinAll();
    std::cout << "Expected runtime: 10 seconds." << std::endl;
}
```

Convience Function for running a list of jobs in a pool, assuming the type being iterated is of `std::function<void(void)>`:
```c++
template <typename Iter>
void RunInPool( Iter begin, Iter end ) {
    ThreadPool pool;
    for( ; begin != end; begin = std::next( begin ) )
        pool.AddJob( *begin );
    pool.JoinAll();
}
```
It's worth noting that the `pool.JoinAll();` is optional in this example, since `JoinAll` is invoked upon object deconstruction. 
