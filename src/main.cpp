#include "engine.hpp"

Engine engine = {};

#define MEASURE_PERF

int main() {
    srand(666);
    
    #ifdef MEASURE_PERF
        engine.render.measureAll = true;
        engine.render.timestampCount = 36;
        #else
        engine.render.measureAll = false;
        engine.render.timestampCount = 2;
    #endif
    
    engine.setup();
    while(!engine.should_close){
        engine.update();
    }
    

    if (engine.render.measureAll){
        printf("%-22s: %7.3f: %7.3f\n", engine.render.timestampNames[0], 0.0, 0.0);
        
        // float timestampPeriod = engine.render.physicalDeviceProperties.limits.timestampPeriod;
        for (int i=1; i<engine.render.currentTimestamp; i++){
            double time_point = double(engine.render.average_ftimestamps[i] - engine.render.average_ftimestamps[0]);
            double time_diff = double(engine.render.average_ftimestamps[i] - engine.render.average_ftimestamps[i-1]);
            printf("%-22s: %7.3f: %7.3f\n", engine.render.timestampNames[i], time_point, time_diff);
        }
    }

    engine.cleanup();
    // do {
    //     cout << '\n' << "Press a key to continue...";}
    // while (cin.get() != '\n');
 
    return 0;
}