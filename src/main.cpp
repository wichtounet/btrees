#include <iostream>

#include "memory.hpp"
#include "test.hpp"
#include "bench.hpp"

int main(int argc, const char* argv[]) {
    std::cout << "Concurrent Binary Trees test" << std::endl;

    //By default launch perf test
    if(argc == 1){
        bench();
    } else if(argc == 2) {
        std::string arg = argv[1];

        if(arg == "-perf"){
            bench();
        } else if(arg == "-test"){
            test();
        } else if(arg == "-memory"){
            test_memory_consumption();
        } else if(arg == "-all"){
            test();
            bench();
            test_memory_consumption();
        } else {
            std::cout << "Unrecognized option " << arg << std::endl;
        }
    } else {
        std::cout << "Too many arguments" << std::endl;
    }

    return 0;
}
