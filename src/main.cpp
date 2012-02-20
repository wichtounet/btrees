#include <iostream>

void test();
void perfTest();

int main(int argc, const char* argv[]) {
    std::cout << "BTrees perf test" << std::endl;

    //By default launch perf test
    if(argc == 1){
        perfTest();
    } else if(argc == 2) {
        std::string arg = argv[1];

        if(arg == "-perf"){
            perfTest();
        } else if(arg == "-test"){
            test();
        } else {
            std::cout << "Unrecognized option " << arg << std::endl;
        }
    } else {
        std::cout << "Too many arguments" << std::endl;
    }

    return 0;
}

void test(){
    std::cout << "Tests the different versions" << std::endl;
}

void perfTest(){
    std::cout << "Tests the performance of the different versions" << std::endl;
}
