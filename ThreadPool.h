#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <array>
#include <list>
#include <functional>
#include <condition_variable>

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

    std::atomic_int         threads_waiting;
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
        std::unique_lock<std::mutex> job_lock( job_available_mutex, std::defer_lock );

        // Quick exit
        if( bailout )
            return false;
        
        queue_mutex.lock();

        // In the case of an empty queue, wait for until we're
        // notified via `job_available_var`. If we're done, bail,
        // otherwise retrieve the next job. 
        if( queue.empty() ) {
            queue_mutex.unlock();

            ++threads_waiting;
            wait_var.notify_all();

            job_lock.lock();
            job_available_var.wait( job_lock, [this]{ return JobsRemaining() || bailout; } );

            --threads_waiting;

            if( bailout ) {
                job_lock.unlock();
                return false;
            }
            queue_mutex.lock();
        }
        else
            job_lock.lock();

        threads[ index ].second = queue.front();
        queue.pop_front();

        queue_mutex.unlock();
        job_lock.unlock();
        
        return true;
    }

public:
    ThreadPool()
        : threads_waiting( 0 )
        , bailout( false )
        , finished( false ) 
    {
        for( unsigned i = 0; i < ThreadCount; ++i ) {
            threads[ i ].first = std::move( std::thread( Task, this, i ) );
            threads[ i ].second = []{};
        }
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
     *  Get the number of jobs left in the queue.
     */
    inline unsigned JobsRemaining() {
        unsigned res = 0;
        queue_mutex.lock();
        res = queue.size();
        queue_mutex.unlock();
        return res;
    }

    /**
     *  Add a new job to the pool. If there are no jobs in the queue,
     *  a thread is woken up to take the job. If all threads are busy,
     *  the job is added to the end of the queue.
     */
    void AddJob( std::function<void(void)> job ) {
        std::lock_guard<std::mutex> guard( queue_mutex );
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
            if( WaitForAll )
                WaitAll();

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
     *  all jobs have finshed executing.
     */
    void WaitAll() {
        if( this->threads_waiting != ThreadCount ) {
            std::unique_lock<std::mutex> lk( wait_mutex );
            wait_var.wait( lk, [this]{ return this->threads_waiting == ThreadCount; } );
            lk.unlock();
        }
    }
};

} // namespace concurrent
} // namespace nbsdx
