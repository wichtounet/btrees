#include <iostream>

#include "skiplist/SkipList.hpp"

typedef skiplist::SkipList<int> SkipList;

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

template<typename T>
void testST(const std::string& name){
    std::cout << "Test single-threaded " << name << std::endl;

    T tree;

    std::cout << "add 1" << std::endl;
    tree.add(1);
    
    std::cout << "contains 1" << std::endl;
    std::cout << "\tresult = " << tree.contains(1) << std::endl;
    
    std::cout << "remove 1" << std::endl;
    std::cout << "\tresult = " << tree.remove(1) << std::endl;
    
    std::cout << "contains 1" << std::endl;
    std::cout << "\tresult = " << tree.contains(1) << std::endl;
}

template<typename T>
void testVersion(const std::string& name){
    testST<T>(name);
}

void test(){
    std::cout << "Tests the different versions" << std::endl;

    testVersion<SkipList>("SkipList");
}

void perfTest(){
    std::cout << "Tests the performance of the different versions" << std::endl;
}
