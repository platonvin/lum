#include <functional>
#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <atomic>
#include <concepts>
#include <condition_variable>
#include <atomic>

#include <glm/ext/vector_int3.hpp>
using glm :: ivec3;

#include "../../containers/thread_pool/thread_pool.hpp"

// Concept to check if callable with ivec3 arg
template <typename Func>
concept ComputeFunc = std::invocable<Func, ivec3>;

class ComputeDispatcher {
public:
    explicit ComputeDispatcher(size_t thread_count = 1)
    // explicit ComputeDispatcher(size_t thread_count = std::thread::hardware_concurrency())
        : thread_count(thread_count) {
        initialize_pool(thread_count);  // Initialize the pool on first usage
    }

    // Dispatch function with concept constraint on ComputeFunction
    void dispatch(const ivec3& size, ComputeFunc auto func) {
        // Determine primary dimension for better load balance
        int primary_dimension = (size.x >= size.y && size.x >= size.z) ? 0 : 
                                (size.y >= size.x && size.y >= size.z) ? 1 : 2;
        int pd = primary_dimension;

        int total_size = (primary_dimension == 0) ? size.x :
                         (primary_dimension == 1) ? size.y : size.z;

        // Calculate chunk size to divide tasks among threads
        int chunk_size = (total_size + thread_count - 1) / thread_count;

        for (size_t i = 0; i < thread_count; ++i) {
            int start = i * chunk_size;
            int end = std::min(start + chunk_size, total_size);

            // Enqueue tasks for each chunk using the thread pool
            pool->enqueue_detach([=, &func]() {
                for (int x = (pd == 0 ? start : 0); x < (pd == 0 ? end : size.x); ++x) {
                for (int y = (pd == 1 ? start : 0); y < (pd == 1 ? end : size.y); ++y) {
                for (int z = (pd == 2 ? start : 0); z < (pd == 2 ? end : size.z); ++z) {
                    func(ivec3{x, y, z});
                }}}
            });
        }

        pool->wait_for_tasks();  // Ensure all tasks are completed before returning
    }

    // Dispatch with collecting returned values (in undefined order)
    template<typename Container, typename ReduceFunc>
    void dispatch(const ivec3& size, ComputeFunc auto func, Container& result, ReduceFunc reduce) {
        // Determine primary dimension for better load balance
        int primary_dimension = (size.x >= size.y && size.x >= size.z) ? 0 :
                                (size.y >= size.x && size.y >= size.z) ? 1 : 2;
        int pd = primary_dimension;

        int total_size = (primary_dimension == 0) ? size.x :
                         (primary_dimension == 1) ? size.y : size.z;

        // Calculate chunk size to divide tasks among threads
        int chunk_size = (total_size + thread_count - 1) / thread_count;

        // Local results for each thread
        std::vector<Container> local_results(thread_count);

        for (size_t i = 0; i < thread_count; ++i) {
            int start = i * chunk_size;
            int end = std::min(start + chunk_size, total_size);

            // Enqueue tasks for each chunk using the thread pool
            pool->enqueue_detach([=, &func, &local_results]() {
                Container local_result; // local result for this thread
                for (int x = (pd == 0 ? start : 0); x < (pd == 0 ? end : size.x); ++x) {
                    for (int y = (pd == 1 ? start : 0); y < (pd == 1 ? end : size.y); ++y) {
                        for (int z = (pd == 2 ? start : 0); z < (pd == 2 ? end : size.z); ++z) {
                            // Call the compute function and store the result
                            auto value = func(ivec3{x, y, z});
                            local_result.push_back(value); // Assuming value can be pushed into the container
                        }
                    }
                }
                local_results[i] = local_result; // Store the local result
            });
        }

        pool->wait_for_tasks();  // Ensure all tasks are completed before returning

        // Reduce all local results into the final result
        for (const auto& local_result : local_results) {
            // Assuming you can merge local_result into result
            for (const auto& val : local_result) {
                result.push_back(val); // Adjust as necessary to fit the container type
            }
        }

        // Perform final reduction (optional, if you need a single result)
        if (!result.empty()) {
            auto final_result = result[0];
            for (size_t i = 1; i < result.size(); ++i) {
                final_result = reduce(final_result, result[i]); // Use your reduce function
            }
            result.clear(); // Clear the container for final result
            result.push_back(final_result); // Store the final reduced result
        }
    }

    static inline std::unique_ptr<dp::thread_pool<dp::details::default_function_type, std::jthread>> pool;
private:
    // Initialize the static thread pool only once
    static void initialize_pool(size_t thread_count) {
        static std::once_flag flag;
        std::call_once(flag, [=]() {
            pool = std::make_unique<dp::thread_pool<dp::details::default_function_type, std::jthread>>(thread_count);
        });
    }

    size_t thread_count;
};

// int main() {
//     ComputeDispatcher dispatcher;

//     dispatcher.dispatch({3, 3, 3}, [](ivec3 pos) {
//         std::cout << "Running task at position (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
//     });

//     return 0;
// }
