#include <iostream>
#include <iomanip>
#include <chrono>
#include <unordered_map>
#include <string>
#include <source_location>
#include <thread>

class Profiler {
public:
    struct Sample {
        std::source_location location;
        double avg_time = 0.0;
        double last_elapsed = 0.0;
    };

    Profiler(size_t max_samples) : max_samples(max_samples), current_index(0) {
        samples.resize(max_samples);
    }

    void startSession() {
        start_time = Clock::now();
        current_index = 0; // reset index for the new session
    }

    void endSession() {    }

    void placeTimestamp(const std::source_location& location = std::source_location::current()) {
        if (current_index >= max_samples) {
            std::cerr << "Profiler: Maximum number of samples reached. Ignoring timestamp.\n";
            return;
        }

        auto now = Clock::now();
        double elapsed = std::chrono::duration<double, std::nano>(now - start_time).count() / 1e6; // ns to ms

        auto& sample = samples[current_index++];
        sample.location = location;
        sample.avg_time = mix(sample.avg_time, elapsed, 0.01);
        sample.last_elapsed = elapsed;
    }

    void printResults() {
        double last_timepoint = 0.0;

        for (size_t i = 0; i < current_index; ++i) {
            const auto& sample = samples[i];
            double delta = sample.avg_time - last_timepoint;

            std::cout << std::fixed << std::setprecision(3)
                      << sample.location.file_name() << ":" << sample.location.line() << " : "
                      << sample.avg_time << " ms : "
                      << delta << " ms (elapsed: " << sample.last_elapsed << " ms)\n";

            last_timepoint = sample.avg_time;
        }
        std::cout << "all results printed;\n"; // clarify no segfault
    }

private:
    using Clock = std::chrono::steady_clock; // Highest precision available?
    using TimePoint = std::chrono::time_point<Clock>;

    TimePoint start_time;
    size_t max_samples;
    size_t current_index;
    std::vector<Sample> samples;
    // aint no way i include glm for this
    static double mix(double a, double b, double factor) {
        return a * (1.0 - factor) + b * factor;
    }
};

// for convenience
// #define PLACE_TIMESTAMP profiler.placeTimestamp()