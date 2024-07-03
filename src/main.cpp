#include "engine.hpp"

Engine engine = {};

int main() {
    srand(666);
    
    engine.setup();
    while(!engine.should_close){
        engine.update();
    }
    engine.cleanup();
    return 0;
}