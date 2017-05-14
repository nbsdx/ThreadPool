#ifndef CONCURRENT_THREADPOOL_H
#define CONCURRENT_THREADPOOL_H

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
    
    std::array<std::thread, ThreadCount> threads;
    std::list<std::function<void(void)>> queue;

    std::atomic_bool        bailout;
    std::atomic_bool        finished;
    std::condition_variable con_with_jobs;
    std::condition_variable con_without_jobs;
    std::mutex              queue_mutex;

    /**
     *  Take the next job in the queue and run it.
     *  Notify the main thread that a job has completed.
     */
    void Task() {
        while( !bailout ) {
            next_job()();
        }
    }

    /**
     *  Get the next job; pop the first item in the queue, 
     *  otherwise wait for a signal from the main thread.
     */
    std::function<void(void)> next_job() {
        std::function<void(void)> res;
        std::unique_lock<std::mutex> job_lock( queue_mutex );

        // Wait for a job if we don't have any.
        con_with_jobs.wait( job_lock, [this]() ->bool { return queue.size() || bailout; } );

        // Get job from the queue
        if( queue.size() ) {
            res = queue.front();
            queue.pop_front();
            con_without_jobs.notify_one();
        }
        else if ( bailout ){ // If we're bailing out, 'inject' a job
            res = []{};
        }
        return res;
    }

public:
    ThreadPool() :
        bailout( false ),
        finished( false )
    {
        for( unsigned i = 0; i < ThreadCount; ++i )
            threads[ i ] = std::thread( [this]{ this->Task(); } );
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
        std::lock_guard<std::mutex> guard( queue_mutex );
        return queue.size();
    }

    /**
     *  Add a new job to the pool. If there are no jobs in the queue,
     *  a thread is woken up to take the job. If all threads are busy,
     *  the job is added to the end of the queue.
     */
    void AddJob( std::function<void(void)> job ) {
        std::lock_guard<std::mutex> guard( queue_mutex );
        queue.emplace_back( job );
        con_with_jobs.notify_one();
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
                WaitAll();
            }

            // note that we're done, and wake up any thread that's
            // waiting for a new job
            bailout = true;
            con_with_jobs.notify_all();

            for( auto &x : threads )
                if( x.joinable() )
                    x.join();
            finished = true;
        }
    }

    /**
     *  Wait for the pool to empty before continuing. 
     *  This does not call `std::thread::join`, it only waits until
     *  all jobs have finshed executing.
     */
    void WaitAll() {
        std::unique_lock<std::mutex> lk( queue_mutex );
        con_without_jobs.wait( lk, [this]{ return queue.empty(); } );
    }
};

} // namespace concurrent
} // namespace nbsdx

#endif //CONCURRENT_THREADPOOL_H
