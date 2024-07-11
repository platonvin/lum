#include "engine.hpp"

Engine engine = {};

int main() {
    srand(666);
    
    
    engine.setup();
    while(!engine.should_close){
        engine.update();
    }
    engine.cleanup();
    
    // do {
    //     cout << '\n' << "Press a key to continue...";}
    // while (cin.get() != '\n');
 
    return 0;
}