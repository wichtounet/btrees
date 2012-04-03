#include <iostream>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>

#include "bench.hpp"
#include "zipf.hpp"
#include "HazardManager.hpp" //To manipulate thread_num

//Include all the trees implementations
#include "skiplist/SkipList.hpp"
#include "nbbst/NBBST.hpp"
#include "avltree/AVLTree.hpp"
#include "lfmst/MultiwaySearchTree.hpp"
#include "cbtree/CBTree.hpp"

//For benchmark
#define OPERATIONS 1000000

//Chrono typedefs
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds milliseconds;
typedef std::chrono::microseconds microseconds;

template<typename Tree, unsigned int Threads>
void random_bench(const std::string& name, unsigned int range, unsigned int add, unsigned int remove){
    Tree tree;

    //TODO Prefill ?

    Clock::time_point t0 = Clock::now();
    
    std::vector<std::thread> pool;
    for(unsigned int i = 0; i < Threads; ++i){
        pool.push_back(std::thread([&tree, range, add, remove, i](){
            thread_num = i;

            unsigned int tid = thread_num;

            std::mt19937_64 engine(time(0) + tid);

            std::uniform_int_distribution<int> valueDistribution(0, range);
            auto valueGenerator = std::bind(valueDistribution, engine);

            std::uniform_int_distribution<int> operationDistribution(0, 99);
            auto operationGenerator = std::bind(operationDistribution, engine);

            for(int i = 0; i < OPERATIONS; ++i){
                unsigned int value = valueGenerator();
                unsigned int op = operationGenerator();

                if(op < add){
                    tree.add(value);
                } else if(op < (add + remove)){
                    tree.remove(value);
                } else {
                    tree.contains(i);
                }
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});

    Clock::time_point t1 = Clock::now();

    milliseconds ms = std::chrono::duration_cast<milliseconds>(t1 - t0);
    unsigned long throughput = (Threads * OPERATIONS) / ms.count();

    std::cout << name << " througput with " << Threads << " threads = " << throughput << " operations / ms" << std::endl;
}

#define BENCH(type, name, range, add, remove)\
    random_bench<type<int, 1>, 1>(name, range, add, remove);\
    random_bench<type<int, 2>, 2>(name, range, add, remove);\
    random_bench<type<int, 3>, 3>(name, range, add, remove);\
    random_bench<type<int, 4>, 4>(name, range, add, remove);\
    random_bench<type<int, 8>, 8>(name, range, add, remove);

void random_bench(unsigned int range, unsigned int add, unsigned int remove){
    std::cout << "Bench with " << OPERATIONS << " operations/thread, range = " << range << ", " << add << "% add, " << remove << "% remove, " << (100 - add - remove) << "% contains" << std::endl;

    BENCH(skiplist::SkipList, "SkipList", range, add, remove);
    BENCH(nbbst::NBBST, "Non-Blocking Binary Search Tree", range, add, remove);
    //BENCH(avltree::AVLTree, "Optimistic AVL Tree", range, add, remove)
    //BENCH(lfmst::MultiwaySearchTree, "Lock-Free Multiway Search Tree", range, add, remove);
    //BENCH(cbtree::CBTree, "Counter Based Tree", range, add, remove);
}

void random_bench(unsigned int range){
    random_bench(range, 50, 50);   //50% put, 50% remove, 0% contains
    random_bench(range, 20, 10);   //20% put, 10% remove, 70% contains
    random_bench(range, 9, 1);     //9% put, 1% remove, 90% contains
}

void random_bench(){
    //random_bench(2000);        //Key in {0, 2000}
    random_bench(200000);        //Key in {0, 200000}
}

template<typename Tree, unsigned int Threads>
void skewed_bench(const std::string& name, unsigned int range, unsigned int add, unsigned int remove, zipf_distribution<>& distribution){
    Tree tree;

    std::mt19937_64 base_engine(time(0));
    //TODO Prefill ?
    for(int i = 0; i < 10000; ++i){
        //tree.add(distribution(base_engine)); 
    }

    Clock::time_point t0 = Clock::now();
    
    std::vector<std::thread> pool;
    for(unsigned int i = 0; i < Threads; ++i){
        pool.push_back(std::thread([&tree, &distribution, range, add, remove, i](){
            thread_num = i;

            unsigned int tid = thread_num;

            std::mt19937_64 engine(time(0) + tid);

            std::uniform_int_distribution<int> operationDistribution(0, 99);
            auto operationGenerator = std::bind(operationDistribution, engine);

            for(int i = 0; i < OPERATIONS; ++i){
                unsigned int value = distribution(engine);
                unsigned int op = operationGenerator();

                if(op < add){
                    tree.add(value);
                } else if(op < (add + remove)){
                    tree.remove(value);
                } else {
                    tree.contains(i);
                }
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});

    Clock::time_point t1 = Clock::now();

    milliseconds ms = std::chrono::duration_cast<milliseconds>(t1 - t0);
    unsigned long throughput = (Threads * OPERATIONS) / ms.count();

    std::cout << name << " througput with " << Threads << " threads = " << throughput << " operations / ms" << std::endl;
}

#define SKEWED_BENCH(type, name, range, add, remove)\
    skewed_bench<type<int, 1>, 1>(name, range, add, remove, distribution);\
    skewed_bench<type<int, 2>, 2>(name, range, add, remove, distribution);\
    skewed_bench<type<int, 3>, 3>(name, range, add, remove, distribution);\
    skewed_bench<type<int, 4>, 4>(name, range, add, remove, distribution);\
    skewed_bench<type<int, 8>, 8>(name, range, add, remove, distribution);

void skewed_bench(unsigned int range, unsigned int add, unsigned int remove, zipf_distribution<>& distribution){
    std::cout << "Skewed Bench with " << OPERATIONS << " operations/thread, range = " << range << ", " << add << "% add, " << remove << "% remove, " << (100 - add - remove) << "% contains" << std::endl;

    SKEWED_BENCH(skiplist::SkipList, "SkipList", range, add, remove);
    SKEWED_BENCH(nbbst::NBBST, "Non-Blocking Binary Search Tree", range, add, remove);
    //BENCH(avltree::AVLTree, "Optimistic AVL Tree", range, add, remove)
    //BENCH(lfmst::MultiwaySearchTree, "Lock-Free Multiway Search Tree", range, add, remove);
    SKEWED_BENCH(cbtree::CBTree, "Counter Based Tree", range, add, remove);
}

void skewed_bench(unsigned int range){
    zipf_distribution<> distribution((double) range, 0, 0.8);
    
    skewed_bench(range, 10, 0, distribution);
}

void skewed_bench(){
    //TODO Perhaps other
    skewed_bench(200000);
}

void duration(Clock::time_point t0, Clock::time_point t1){
    microseconds us = std::chrono::duration_cast<microseconds>(t1 - t0);

    if(us.count() < 100000){
        std::cout << us.count() << "us";
    } else {
        milliseconds ms = std::chrono::duration_cast<milliseconds>(t1 - t0);

        std::cout << ms.count() << "ms";
    }
}

template<typename Tree, unsigned int Threads>
void seq_construction_bench(const std::string& name, unsigned int size){
    Tree tree;

    Clock::time_point t0 = Clock::now();
    
    unsigned int part = size / Threads;

    std::vector<std::thread> pool;
    for(unsigned int i = 0; i < Threads; ++i){
        pool.push_back(std::thread([&tree, part, size, i](){
            thread_num = i;

            unsigned int tid = thread_num;

            for(unsigned int i = tid * part; i < (tid + 1) * part; ++i){
                tree.add(i);
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});

    Clock::time_point t1 = Clock::now();

    std::cout << "Construction of " << name << " with " << size << " elements took ";
    duration(t0, t1); 
    std::cout << " with " << Threads << " threads" << std::endl;

    //Empty the tree
    for(unsigned int i = 0; i < size; ++i){
        tree.remove(i);
    }
}

#define SEQ_CONSTRUCTION(type, name, size)\
    seq_construction_bench<type<int, 1>, 1>(name, size);\
    seq_construction_bench<type<int, 2>, 2>(name, size);\
    seq_construction_bench<type<int, 3>, 3>(name, size);\
    seq_construction_bench<type<int, 4>, 4>(name, size);\
    seq_construction_bench<type<int, 8>, 8>(name, size);

void seq_construction_bench(){
    std::cout << "Bench the sequential construction time of each data structure" << std::endl;

    std::vector<int> sizes = {50000, 100000, 500000, 1000000, 5000000, 10000000, 20000000};

    for(auto size : sizes){
        SEQ_CONSTRUCTION(skiplist::SkipList, "SkipList", size);
        SEQ_CONSTRUCTION(nbbst::NBBST, "NBBST", size);
    }

    //TODO Continue that
}

template<typename Tree, unsigned int Threads>
void random_construction_bench(const std::string& name, unsigned int size){
    Tree tree;

    Clock::time_point t0 = Clock::now();

    std::vector<int> elements;
    for(unsigned int i = 0; i < size; ++i){
        elements.push_back(i);
    }

    random_shuffle(elements.begin(), elements.end());
    
    unsigned int part = size / Threads;

    std::vector<std::thread> pool;
    for(unsigned int i = 0; i < Threads; ++i){
        pool.push_back(std::thread([&tree, &elements, part, size, i](){
            thread_num = i;

            unsigned int tid = thread_num;

            for(unsigned int i = tid * part; i < (tid + 1) * part; ++i){
                tree.add(elements[i]);
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});

    Clock::time_point t1 = Clock::now();

    std::cout << "Construction of " << name << " with " << size << " elements took ";
    duration(t0, t1);
    std::cout << " with " << Threads << " threads" << std::endl;

    //Empty the tree
    for(unsigned int i = 0; i < size; ++i){
        tree.remove(i);
    }
}

#define RANDOM_CONSTRUCTION(type, name, size)\
    random_construction_bench<type<int, 1>, 1>(name, size);\
    random_construction_bench<type<int, 2>, 2>(name, size);\
    random_construction_bench<type<int, 3>, 3>(name, size);\
    random_construction_bench<type<int, 4>, 4>(name, size);\
    random_construction_bench<type<int, 8>, 8>(name, size);

void random_construction_bench(){
    std::cout << "Bench the random construction time of each data structure" << std::endl;

    std::vector<int> sizes = {50000, 100000, 500000, 1000000, 5000000, 10000000, 20000000};

    for(auto size : sizes){
        RANDOM_CONSTRUCTION(skiplist::SkipList, "SkipList", size);
        RANDOM_CONSTRUCTION(nbbst::NBBST, "NBBST", size);
    }

    //TODO Continue that
}

#define SEARCH_BENCH_OPERATIONS 100000

template<typename Tree, unsigned int Threads>
void search_bench(const std::string& name, unsigned int size){
    Tree tree;

    //TODO Test if it is more interesting to insert them in the exact range or in the complete range
    std::vector<int> elements;
    for(unsigned int i = 0; i < size; ++i){
        elements.push_back(i);
    }

    random_shuffle(elements.begin(), elements.end());
            
    for(unsigned int i = 0; i < size; ++i){
        tree.add(elements[i]);
    }
    
    Clock::time_point t0 = Clock::now();

    std::vector<std::thread> pool;
    for(unsigned int i = 0; i < Threads; ++i){
        pool.push_back(std::thread([&tree, size, i](){
            thread_num = i;
            
            std::mt19937_64 engine(time(0) + i);

            std::uniform_int_distribution<int> valueDistribution(0, size);
            auto valueGenerator = std::bind(valueDistribution, engine);

            for(int s = 0; s < SEARCH_BENCH_OPERATIONS; ++s){
                tree.contains(valueGenerator());
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});

    Clock::time_point t1 = Clock::now();
    
    milliseconds ms = std::chrono::duration_cast<milliseconds>(t1 - t0);
    unsigned long throughput = (Threads * SEARCH_BENCH_OPERATIONS) / ms.count();

    std::cout << name << "-" << size << " search througput with " << Threads << " threads = " << throughput << " operations / ms" << std::endl;

    //Empty the tree
    for(unsigned int i = 0; i < size; ++i){
        tree.remove(i);
    }
}


#define SEARCH(type, name, size)\
    search_bench<type<int, 1>, 1>(name, size);\
    search_bench<type<int, 2>, 2>(name, size);\
    search_bench<type<int, 3>, 3>(name, size);\
    search_bench<type<int, 4>, 4>(name, size);\
    search_bench<type<int, 8>, 8>(name, size);

void search_bench(){
    std::cout << "Bench the search performances of each data structure" << std::endl;

    std::vector<int> sizes = {50000, 100000, 500000, 1000000, 5000000, 10000000, 20000000};

    for(auto size : sizes){
        SEARCH(skiplist::SkipList, "SkipList", size);
        SEARCH(nbbst::NBBST, "NBBST", size);
    }

    //TODO Continue that
}

void bench(){
    std::cout << "Tests the performance of the different versions" << std::endl;

    //Launch the random benchmark
    //random_bench();
    skewed_bench();

    //Launch the construction benchmark
    //seq_construction_bench();
    //random_construction_bench();

    //Launch the search benchmark
    //search_bench();
}
