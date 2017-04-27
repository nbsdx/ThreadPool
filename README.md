# ThreadPool
Lightweight, Generic, Pure C++11 ThreadPool

## Rational
I needed a Thread Pool for something I was writing, and I didn't see any that I liked. This is still somewhat a work in progress; it's stable, but there are probably places where some of the locking logic could be better. `ThreadPool::JoinAll` is a little sloppy, but it works. 

## Licensing 
Public Domain. If my licensing is wrong, please let me know. Use at your own risk for whatever you want. Feel free to change the namespaces as well. Apparently licensing is hard and complicated. If your country doesn't have a public domain, feel free to say you found this on the side of the road. 

## Overview
`ThreadPool` is a super simple class that manages threads and jobs. `ThreadCount` threads are created at object instantiation time, and persist until the `ThreadPool` object is destroyed. You cannot change the thread count. A later version may allow you to set the thread count through the constructor rather than as a template parameter, but it's not something I care to do at the moment. Jobs are functions with no parameters or return values. This decision was to make it as generic as possible so it could be integrated into a variety of projects. If you can't get your job to work with those constraints, you're doing something wrong, or you need to roll your own ThreadPool. But you're probably making things overly complicated.

Below is a quick overview, but ThreadPool.h is documented, so just read that. It's less than 200 lines with comments.

```c++
class CallObject{
public:
	CallObject(){func = [](DataType){};};
	CallObject(std::function<void(DataType)> in1, DataType in2):
		func(in1),data(in2){};
	std::function<void(DataType)> func;
	DataType data;
};

template <typename DataType, unsigned ThreadCount = 20, unsigned Max = 10000>
class ThreadPool {
public:
    ThreadPool();
    ~ThreadPool();
    void AddJob( CallObject<DataType> job );
	int AddJobNoMoreThanMax( CallObject<DataType> job );
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
    
    ThreadPool<int> pool; // Defaults to 20 threads,10000 queue size.
    int JOB_COUNT = 20000;
    
    for( int i = 0; i < JOB_COUNT; ++i )
	{
        if(pool.AddJobNoMoreThanMax(CallObject<int> job) < 0 )
			break;
	}
    
    pool.JoinAll();
    std::cout << "Expected runtime: x seconds." << std::endl;
}
```

Convience Function for running a list of jobs in a pool, assuming the type being iterated is of `std::function<void(void)>`:
```c++
template <typename Iter, unsigned Count = 10>
void RunInPool( Iter begin, Iter end ) {
    ThreadPool<CallObject<your_type_name>> pool;
    for( ; begin != end; begin = std::next( begin ) )
        pool.AddJob( *begin );
    pool.JoinAll();
}
```
It's worth nothing that the `pool.JoinAll();` is optional in this example, since `JoinAll` is invoked upon object deconstruction. 
