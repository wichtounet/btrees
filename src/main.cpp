#include <iostream>
#include <cassert>
#include <vector>
#include <algorithm>

#include <sys/time.h> //TODO It's not portable...

#include <omp.h>

#include "Constants.hpp"

//Include all the trees implementations
#include "skiplist/SkipList.hpp"
#include "nbbst/NBBST.hpp"
#include "avltree/AVLTree.hpp"

//Typedefs for the different versions
typedef skiplist::SkipList<int> SkipList;
typedef nbbst::NBBST<int> NBBST;
typedef avltree::AVLTree<int> AVLTree;

void test();
void perfTest();

int main(int argc, const char* argv[]) {
    std::cout << "Concurrent Binary Trees test" << std::endl;

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
    
    srand(time(NULL));
    
    T tree;

    //Insert sequential numbers
    for(unsigned int i = 0; i < N; ++i){
        assert(!tree.contains(i));
        assert(tree.add(i));
        assert(tree.contains(i));
    }
    
    //Remove all the sequential numbers
    for(unsigned int i = 0; i < N; ++i){
        assert(tree.contains(i));
        assert(tree.remove(i));     
        assert(!tree.contains(i));
    }
    
    //Verify again that all the numbers have been removed
    for(unsigned int i = 0; i < N; ++i){
        assert(!tree.contains(i));
    }

    std::vector<int> rand;

    //Insert N random numbers in the tree
    for(unsigned int i = 0; i < N; ++i){
        int number = random();

        if(tree.contains(number)){
            assert(!tree.add(number));     
            assert(tree.contains(number));
        } else {
            assert(tree.add(number));     
            assert(tree.contains(number));

            rand.push_back(number);
        }
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
    for(int number : rand){
        assert(tree.contains(number));
        assert(tree.remove(number));
    }

    std::cout << "Test passed successfully" << std::endl;
}

template<typename T, unsigned int Threads>
void testMT(){
    std::cout << "Test with " << Threads << " threads" << std::endl;

    T tree;

    omp_set_num_threads(Threads);

    #pragma omp parallel shared(tree)
    {
        unsigned int tid = omp_get_thread_num();

        //Insert sequential numbers
        for(unsigned int i = tid * N; i < (tid + 1) * N; ++i){
            assert(tree.add(i));
            assert(tree.contains(i));
        }

        //Remove all the sequential numbers
        for(unsigned int i = tid * N; i < (tid + 1) * N; ++i){
            assert(tree.contains(i));
            assert(tree.remove(i));   
        }
    }
    
    #pragma omp parallel shared(tree)
    {
        //Verify that every numbers has been removed correctly
        for(unsigned int i = 0; i < Threads * N; ++i){
            assert(!tree.contains(i));
        }
    }
    
    std::cout << "Test with " << Threads << " threads passed succesfully" << std::endl;
}

template<typename T>
void testMT(const std::string& name){
    std::cout << "Test multi-threaded (with " << N << " elements) " << name << std::endl;
    
    testMT<T, 2>();
    testMT<T, 3>();
    testMT<T, 4>();
    testMT<T, 6>();
    testMT<T, 8>();
    testMT<T, 12>();
    testMT<T, 16>();
    testMT<T, 32>();
}

template<typename T>
void testVersion(const std::string& name){
    testST<T>(name);
    testMT<T>(name);
}

void test(){
    std::cout << "Tests the different versions" << std::endl;

    //testVersion<SkipList>("SkipList");
    //testVersion<NBBST>("Non-Blocking Binary Search Tree");
    testVersion<AVLTree>("Optimistic AVL Tree");
}

template<typename Tree, unsigned int Threads>
void bench(const std::string& name, unsigned int range, unsigned int add, unsigned int remove){
    timespec ts1 = {0,0};
    timespec ts2 = {0,0};

    Tree tree;

    omp_set_num_threads(Threads);

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts1);

    #pragma omp parallel shared(tree)
    {
        for(int i = 0; i < OPERATIONS; ++i){
            unsigned int value = random(range);

            unsigned int op = random(100);

            if(op < add){
                tree.add(value);
            } else if(op < (add + remove)){
                tree.remove(value);
            } else {
                tree.contains(i);
            }
        }
    }
    
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts2);

    unsigned long duration = (ts2.tv_sec - ts1.tv_sec) * 1000000000l + (ts2.tv_nsec - ts1.tv_nsec);
    unsigned long operations = Threads * OPERATIONS;
    unsigned long throughput = duration / operations;

    //std::cout << (Threads * OPERATIONS) << " operations on " << name << " took " << duration << "ns" << std::endl;
    std::cout << name << " througput with " << Threads << " threads = " << throughput << " operations / ms" << std::endl;
}

template<typename Tree>
void bench(const std::string& name, unsigned int range, unsigned int add, unsigned int remove){
    bench<Tree, 1>(name, range, add, remove);
    bench<Tree, 2>(name, range, add, remove);
    bench<Tree, 4>(name, range, add, remove);
    bench<Tree, 8>(name, range, add, remove);
}

void bench(unsigned int range, unsigned int add, unsigned int remove){
    std::cout << "Bench with " << OPERATIONS << " operations/thread, range = " << range << ", " << add << "% add, " << remove << "% remove, " << (100 - add - remove) << "% contains" << std::endl;

    //bench<SkipList>("SkipList", range, add, remove);
    //bench<NBBST>("Non-blocking Binary Search Tree", range, add, remove);
    bench<AVLTree>("Optimistic AVL Tree", range, add, remove);
}

void bench(unsigned int range){
    bench(range, 50, 50);   //50% put, 50% remove, 0% contains
    bench(range, 20, 10);   //20% put, 10% remove, 70% contains
    bench(range, 9, 1);     //9% put, 1% remove, 90% contains
}

void perfTest(){
    std::cout << "Tests the performance of the different versions" << std::endl;

    bench(2000);            //Key in {0, 2000}
    //bench(200000);        //Key in {0, 200000}
}
