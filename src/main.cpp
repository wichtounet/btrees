#include <iostream>
#include <cassert>
#include <vector>
#include <algorithm>

#include "Constants.hpp"

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
    std::cout << "Test single-threaded (with " << N << " elements) " << name << std::endl;
    
    T tree;

    //Insert sequential numbers
    for(unsigned int i = 0; i < N; ++i){
        assert(!tree.contains(i));
        tree.add(i);     
        assert(tree.contains(i));
    }
    
    //Remove all the sequential numbers
    for(unsigned int i = 0; i < N; ++i){
        assert(tree.contains(i));
        assert(tree.remove(i));     
        assert(!tree.contains(i));
    }

    std::vector<int> rand;

    //Insert random numbers in the tree
    for(unsigned int i = 0; i < N; ++i){
        int number = random();

        tree.add(number);     
        assert(tree.contains(number));

        rand.push_back(number);
    }
    
    //Try remove when the number is not in the tree
    for(unsigned int i = 0; i < N; ++i){
        int number = random();

        if(!tree.contains(number)){
            assert(!tree.remove(number));
            assert(!tree.contains(number));
        }
    }

    //Avoid removing in the same order
    random_shuffle(rand.begin(), rand.end());

    //Verify that we can remove all the numbers from the tree
    for(unsigned int i = 0; i < N; ++i){
        int number = rand[i];

        assert(tree.contains(number));
        assert(tree.remove(number));
    }

    std::cout << "Test passed successfully" << std::endl;
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
