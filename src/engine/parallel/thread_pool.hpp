#include <thread>
#include <functional>
#include <mutex>
#include <atomic>
#include <cassert>

using std::atomic, std::mutex;

class thread_pool {
public:
    // Singleton instance for thread pool
    static thread_pool& instance() {
        static thread_pool instance = {};  // Guaranteed to be destroyed, instantiated on first use
        return instance;
    }

    // Returns the number of threads in the pool
    int thread_count() const {
        return num_threads;
    }

    // Dispatch a task to the pool, ensuring all threads execute it
    void dispatch(const std::function<void(int)>& func) {
        while(true) {
            if(threads_active == 0) break;
        };

        // NOTE: MUST happen first cause otherwise threads will read invalid one
        current_task = func;

        // NOTE: MUST happen before setting flags (so second)
        // because otherwise threads are going to decrement already zero counter
        threads_active = num_threads; // Set the task counter to the number of threads
        
        // Set the flags to signal all threads to run the task
        // Note: each =true causes one thread to run task (only) one time
        for (int i = 0; i < num_threads; i++) {
            thread_flags[i] = true;
        }

        // Wait for all threads to finish the task
        while(true) {
            int read_counter = threads_active;
            // assert(read_counter >= 0);
            if(read_counter == 0) {
                break;
            }
        }
    }

private:
    // Constructor: allocate threads and set flags to idle
    thread_pool() : threads_active(0), should_stop(false) {
        
        // num_threads = std::thread::hardware_concurrency() - 4;
        num_threads = std::thread::hardware_concurrency()/2;
        
        if (num_threads <= 0) {
            num_threads = 1;
        }

        // Allocate arrays for thread management
        threads = new std::thread[num_threads];
        thread_flags = new atomic<bool>[num_threads];

        // Initialize flags to false (idle)
        for (int i = 0; i < num_threads; i++) {
            thread_flags[i] = false;
        }

        // Launch worker threads
        for (int i = 0; i < num_threads; i++) {
            threads[i] = std::thread([this, i]() {
                this->worker_thread_fun(i);
            });
        }
    }

    // Destructor: Clean up resources and join threads
    ~thread_pool() {
        {
            should_stop = true;  // Set stop flag to true

            // wait for all threads to return 
            while(threads_active > 0) {};
        }

        // Join all threads to ensure clean termination
        for (int i = 0; i < num_threads; i++) {
            if (threads[i].joinable()) {
                threads[i].join();  // Ensure each thread completes
            }
        }

        // Clean up allocated memory
        delete[] threads;
        delete[] thread_flags;
    }

    // Worker thread function that executes tasks
    void worker_thread_fun(int thread_id) {
        // waiting for actual work to be submitted
        while (true) {

            std::function<void(int)> task = {};

            if (should_stop) break;  // If stop flag is set, exit the loop

            if (thread_flags[thread_id]) {
                task = current_task;  // Get the current task
                thread_flags[thread_id] = false;  // Reset the flag (task is being processed)
            }

            if (task) {
                task(thread_id);  // Execute the task

                // After task completion, decrement the counter. When it reaches 0, it means that all threads are done with that task 
                threads_active -= 1;
            }
        }
    }

    std::thread* threads;  // Array of worker threads
    int num_threads = 0;  // Number of threads in the pool
    std::function<void(int)> current_task;  // The current task to be executed
    
    alignas(std::hardware_destructive_interference_size) 
        // Array of flags indicating whether each thread has a task
        // each thread flag is like a switch - you set it to true from main thread, and in 
        atomic<bool>* thread_flags;  
    alignas(std::hardware_destructive_interference_size)
        atomic<int> threads_active = 0;  // Counter to track the number of remaining tasks
    alignas(std::hardware_destructive_interference_size)
        atomic<bool> should_stop = false;  // Flag to signal stopping the pool
};