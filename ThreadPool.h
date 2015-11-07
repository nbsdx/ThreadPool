#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <array>
#include <list>
#include <functional>
#include <condition_variable>

#include <iostream>

namespace nbsdx {
namespace concurrent {

/**
 *  Simple ThreadPool that creates `ThreadCount` threads upon its creation,
 *  and pulls from a queue to get new jobs. The default is 10 threads.
 *
 *  This class requires a number of c++11 features be present in your compiler.
 */
template <unsigned ThreadCount = 10>
class ThreadPool {
    
    std::array<
        std::pair<std::thread,
                  std::function<void(void)>>,
        ThreadCount> threads;

    std::list<std::function<void(void)>> queue;

    std::atomic_bool        bailout;
    std::atomic_bool        finished;
    std::condition_variable job_available_var;
    std::condition_variable wait_var;
    std::mutex              job_available_mutex;
    std::mutex              wait_mutex;
    std::mutex              queue_mutex;

    /**
     *  Our wrapper that executes user jobs without exiting until being
     *  told to no longer grab new jobs.
     */
    static void Task( ThreadPool<ThreadCount> *pool, unsigned index ) {
        while( pool->take_next( index ) )
            pool->threads[ index ].second();
    }

    /**
     *  Take the job at the front of the queue. 
     *  Returns true if it got a job, false otherwise.
     */
    bool take_next( unsigned index ) {
        bool should_wait = false;

        // Quick exit
        if( bailout )
            return false;
        
        queue_mutex.lock();

        // In the case of an empty queue, wait for until we're
        // notified via `job_available_var`. If we're done, bail,
        // otherwise retrieve the next job. 
        if( queue.empty() ) {
            queue_mutex.unlock();

            std::unique_lock<std::mutex> lk( job_available_mutex );
            job_available_var.wait( lk );
            lk.unlock();

            if( bailout )
                return false;

            queue_mutex.lock();
        }
        
        threads[ index ].second = queue.front();
        queue.pop_front();

        // If we're waiting for the queue to be empty,
        // stop waiting.
        if( queue.empty() )
            wait_var.notify_one();

        queue_mutex.unlock();
        return true;
    }

public:
    ThreadPool() : bailout( false ), finished( false ) {
        for( unsigned i = 0; i < ThreadCount; ++i )
            threads[ i ].first = std::move( std::thread( Task, this, i ) );
    }

    /**
     *  JoinAll on deconstruction
     */
    ~ThreadPool() {
        JoinAll();
    }

    /**
     *  Get the number of threads in this pool
     */
    inline unsigned Size() const {
        return ThreadCount;
    }

    /**
     *  Add a new job to the pool. If there are no jobs in the queue,
     *  a thread is woken up to take the job. If all threads are busy,
     *  the job is added to the end of the queue.
     */
    void AddJob( std::function<void(void)> job ) {
        queue.emplace_back( job );
        job_available_var.notify_one();
    }

    /**
     *  Join with all threads. Block until all threads have completed.
     *  Params: WaitForAll: If true, will wait for the queue to empty 
     *          before joining with threads. If false, will complete
     *          current jobs, then inform the threads to exit.
     *  The queue will be empty after this call, and the threads will
     *  be done. After invoking `ThreadPool::JoinAll`, the pool can no
     *  longer be used. If you need the pool to exist past completion
     *  of jobs, look to use `ThreadPool::WaitAll`.
     */
    void JoinAll( bool WaitForAll = true ) {
        if( !finished ) {
            if( WaitForAll ) {
                queue_mutex.lock();
                if( !queue.empty() ) {
                    queue_mutex.unlock();
                    WaitAll();
                }
                else
                    queue_mutex.unlock();
            }

            // note that we're done, and wake up any thread that's
            // waiting for a new job
            bailout = true;
            job_available_var.notify_all();

            for( auto &x : threads )
                if( x.first.joinable() )
                    x.first.join();
            finished = true;
        }
    }

    /**
     *  Wait for the pool to empty before continuing. 
     *  This does not call `std::thread::join`, it only waits until
     *  all jobs have been removed from the queue. As such, jobs
     *  may still be active when WaitAll returns. 
     */
    void WaitAll() {
        std::unique_lock<std::mutex> lk( wait_mutex );
        wait_var.wait( lk );
        lk.unlock();
    }
};

} // namespace concurrent
} // namespace nbsdx
