#include <iostream>
#include <sstream>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>
#include <set>
#include <vector>

#include "bench.hpp"
#include "zipf.hpp"             //To generate zipf distributed numbers
#include "HazardManager.hpp"    //To manipulate thread_num
#include "Results.hpp"          //To generate the graphs data

//Include all the trees implementations
#include "skiplist/SkipList.hpp"
#include "nbbst/NBBST.hpp"
#include "avltree/AVLTree.hpp"
#include "lfmst/MultiwaySearchTree.hpp"
#include "cbtree/CBTree.hpp"

//For benchmark
#define OPERATIONS 1000000
#define REPEAT 12
#define SEARCH_BENCH_OPERATIONS 100000 //TODO Perhaps a bit few...

//Chrono typedefs
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds milliseconds;
typedef std::chrono::microseconds microseconds;

template<typename Tree, unsigned int Threads>
void random_bench(const std::string& name, unsigned int range, unsigned int add, unsigned int remove, Results& results){
    Tree tree;

    //TODO Prefill ?

    Clock::time_point t0 = Clock::now();

    std::vector<int> elements[Threads];
    
    std::vector<std::thread> pool;
    for(unsigned int tid = 0; tid < Threads; ++tid){
        pool.push_back(std::thread([&tree, &elements, range, add, remove, tid](){
            thread_num = tid;

            std::mt19937_64 engine(time(0) + tid);

            std::uniform_int_distribution<int> valueDistribution(0, range);
            auto valueGenerator = std::bind(valueDistribution, engine);

            std::uniform_int_distribution<int> operationDistribution(0, 99);
            auto operationGenerator = std::bind(operationDistribution, engine);

            for(int i = 0; i < OPERATIONS; ++i){
                unsigned int value = valueGenerator();
                unsigned int op = operationGenerator();

                if(op < add){
                    if(tree.add(value)){
                        elements[thread_num].push_back(value);
                    }
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

    results.add_result(name, throughput);

    pool.clear();
    for(unsigned int tid = 0; tid < Threads; ++tid){
        pool.push_back(std::thread([&tree, &elements, tid](){
            thread_num = tid;

            for(auto i : elements[thread_num]){
                tree.remove(i);
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});
}

#define BENCH(type, name, range, add, remove)\
    random_bench<type<int, 1>, 1>(name, range, add, remove, results);\
    random_bench<type<int, 2>, 2>(name, range, add, remove, results);\
    random_bench<type<int, 3>, 3>(name, range, add, remove, results);\
    random_bench<type<int, 4>, 4>(name, range, add, remove, results);\
    random_bench<type<int, 8>, 8>(name, range, add, remove, results);\
    random_bench<type<int, 16>, 16>(name, range, add, remove, results);\
    random_bench<type<int, 32>, 32>(name, range, add, remove, results);

void random_bench(unsigned int range, unsigned int add, unsigned int remove){
    std::cout << "Bench with " << OPERATIONS << " operations/thread, range = " << range << ", " << add << "% add, " << remove << "% remove, " << (100 - add - remove) << "% contains" << std::endl;

    std::stringstream bench_name;
    bench_name << "random-" << range << "-" << add << "-" << remove;

    Results results;
    results.start(bench_name.str());
    results.set_max(7);

    for(int i = 0; i < REPEAT; ++i){
        BENCH(skiplist::SkipList, "skiplist", range, add, remove);
        BENCH(nbbst::NBBST, "nbbst", range, add, remove);
        BENCH(avltree::AVLTree, "avltree", range, add, remove)
        //TODO BENCH(lfmst::MultiwaySearchTree, "lfmst", range, add, remove);
        BENCH(cbtree::CBTree, "cbtree", range, add, remove);
    }

    results.finish();
}

void random_bench(unsigned int range){
    random_bench(range, 50, 50);   //50% put, 50% remove, 0% contains
    random_bench(range, 20, 10);   //20% put, 10% remove, 70% contains
    random_bench(range, 9, 1);     //9% put, 1% remove, 90% contains
}

void random_bench(){
    random_bench(200);                                  //Key in {0, 200}
    //random_bench(2000);                                 //Key in {0, 2000}
    //random_bench(20000);                                //Key in {0, 20000}
    //random_bench(std::numeric_limits<int>::max());      //Key in {0, 2^32}
}

template<typename Tree, unsigned int Threads>
void skewed_bench(const std::string& name, unsigned int range, unsigned int add, unsigned int remove, zipf_distribution<>& distribution, Results& results){
    Tree tree;

    Clock::time_point t0 = Clock::now();
    
    std::vector<std::thread> pool;
    for(unsigned int tid = 0; tid < Threads; ++tid){
        pool.push_back(std::thread([&tree, &distribution, range, add, remove, tid](){
            thread_num = tid;

            std::mt19937_64 engine(time(0) + tid);

            std::uniform_int_distribution<int> operationDistribution(0, 99);
            auto operationGenerator = std::bind(operationDistribution, engine);
            
            for(int i = 0; i < OPERATIONS; ++i){
                unsigned int value = distribution(engine);
                tree.add(value);
            }

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
    unsigned long throughput = (Threads * 2 * OPERATIONS) / ms.count();

    std::cout << name << " througput with " << Threads << " threads = " << throughput << " operations / ms" << std::endl;
    results.add_result(name, throughput);
}

void skewed_bench(unsigned int range, unsigned int add, unsigned int remove, zipf_distribution<>& distribution, Results& results){
    std::cout << "Skewed Bench with " << OPERATIONS << " operations/thread, range = " << range << ", " << add << "% add, " << remove << "% remove, " << (100 - add - remove) << "% contains" << std::endl;

    //skewed_bench<skiplist::SkipList<int, 8>, 8>("skiplist", range, add, remove, distribution, results);
    skewed_bench<nbbst::NBBST<int, 8>, 8>("nbbst", range, add, remove, distribution, results);
    skewed_bench<avltree::AVLTree<int, 8>, 8>("avltree", range, add, remove, distribution, results);
    //skewed_bench<lfmst::MultiwaySearchTree<int, 8>, 8>("lfmst", range, add, remove, distribution, results);
    skewed_bench<cbtree::CBTree<int, 8>, 8>("cbtree", range, add, remove, distribution, results);
}

void skewed_bench(unsigned int range){
    std::stringstream name;
    name << "skewed-" << range;

    Results results;
    results.start(name.str());

    zipf_distribution<> distribution((double) range, 0, 0.8);
    skewed_bench(range, 10, 0, distribution, results);
    
    /*distribution = zipf_distribution<>((double) range, 0, 1.0);
    skewed_bench(range, 10, 0, distribution, results);
    
    distribution = zipf_distribution<>((double) range, 0, 1.2);
    skewed_bench(range, 10, 0, distribution, results);*/
    
    distribution = zipf_distribution<>((double) range, 0, 1.4);
    skewed_bench(range, 10, 0, distribution, results);
    
    distribution = zipf_distribution<>((double) range, 0, 1.6);
    skewed_bench(range, 10, 0, distribution, results);

    results.finish();
}

void skewed_bench(){
    //TODO Perhaps other
    //skewed_bench(2000);
    //skewed_bench(20000);
    //skewed_bench(200000);
    skewed_bench(2000000);
}

unsigned long get_duration(Clock::time_point t0, Clock::time_point t1){
    milliseconds ms = std::chrono::duration_cast<milliseconds>(t1 - t0);

    return ms.count();
}

template<typename Tree, unsigned int Threads>
void seq_construction_bench(const std::string& name, unsigned int size, Results& results){
    Tree tree;

    Clock::time_point t0 = Clock::now();
    
    unsigned int part = size / Threads;

    std::vector<std::thread> pool;
    for(unsigned int tid = 0; tid < Threads; ++tid){
        pool.push_back(std::thread([&tree, part, size, tid](){
            thread_num = tid;

            for(unsigned int i = tid * part; i < (tid + 1) * part; ++i){
                tree.add(i);
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});

    Clock::time_point t1 = Clock::now();

    std::cout << "Construction of " << name << " with " << size << " elements took " << get_duration(t0, t1) << " ms with " << Threads << " threads" << std::endl;
    results.add_result(name, get_duration(t0, t1));

    //Empty the tree
    for(unsigned int i = 0; i < size; ++i){
        tree.remove(i);
    }
}

#define SEQ_CONSTRUCTION(type, name, size)\
    seq_construction_bench<type<int, 1>, 1>(name, size, results);\
    seq_construction_bench<type<int, 2>, 2>(name, size, results);\
    seq_construction_bench<type<int, 3>, 3>(name, size, results);\
    seq_construction_bench<type<int, 4>, 4>(name, size, results);\
    seq_construction_bench<type<int, 8>, 8>(name, size, results);

void seq_construction_bench(){
    std::cout << "Bench the sequential construction time of each data structure" << std::endl;
    
    std::vector<int> small_sizes = {1000, 5000, 10000};
    
    for(auto size : small_sizes){
        std::stringstream name;
        name << "sequential-build-" << size;

        Results results;
        results.start(name.str());
        results.set_max(5);

        for(int i = 0; i < REPEAT; ++i){
            SEQ_CONSTRUCTION(skiplist::SkipList, "skiplist", size);
            SEQ_CONSTRUCTION(nbbst::NBBST, "nbbst", size);
            SEQ_CONSTRUCTION(avltree::AVLTree, "avltree", size);
            //SEQ_CONSTRUCTION(lfmst::MultiwaySearchTree, "Multiway Search Tree", size);
            SEQ_CONSTRUCTION(cbtree::CBTree, "cbtree", size);
        }

        results.finish();
    }

    std::vector<int> sizes = {50000, 100000, 500000, 1000000, 5000000, 10000000};

    for(auto size : sizes){
        std::stringstream name;
        name << "sequential-build-" << size;

        Results results;
        results.start(name.str());
        results.set_max(5);
        
        for(int i = 0; i < REPEAT; ++i){
            SEQ_CONSTRUCTION(skiplist::SkipList, "skiplist", size);
            //Too slow SEQ_CONSTRUCTION(nbbst::NBBST, "nbbst", size);
            SEQ_CONSTRUCTION(avltree::AVLTree, "avltree", size);
            //SEQ_CONSTRUCTION(lfmst::MultiwaySearchTree, "Multiway Search Tree", size);
            SEQ_CONSTRUCTION(cbtree::CBTree, "cbtree", size);
        }

        results.finish();
    }
}

template<typename Tree, unsigned int Threads>
void random_construction_bench(const std::string& name, unsigned int size, Results& results){
    Tree tree;

    std::vector<int> elements;
    for(unsigned int i = 0; i < size; ++i){
        elements.push_back(i);
    }

    random_shuffle(elements.begin(), elements.end());

    Clock::time_point t0 = Clock::now();
    
    unsigned int part = size / Threads;

    std::vector<std::thread> pool;
    for(unsigned int tid = 0; tid < Threads; ++tid){
        pool.push_back(std::thread([&tree, &elements, part, size, tid](){
            thread_num = tid;

            for(unsigned int i = tid * part; i < (tid + 1) * part; ++i){
                tree.add(elements[i]);
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});

    Clock::time_point t1 = Clock::now();

    std::cout << "Construction of " << name << " with " << size << " elements took " << get_duration(t0, t1) << " ms with " << Threads << " threads" << std::endl;
    results.add_result(name, get_duration(t0, t1));

    //Empty the tree
    for(unsigned int i = 0; i < size; ++i){
        tree.remove(i);
    }
}

#define RANDOM_CONSTRUCTION(type, name, size)\
    random_construction_bench<type<int, 1>, 1>(name, size, results);\
    random_construction_bench<type<int, 2>, 2>(name, size, results);\
    random_construction_bench<type<int, 3>, 3>(name, size, results);\
    random_construction_bench<type<int, 4>, 4>(name, size, results);\
    random_construction_bench<type<int, 8>, 8>(name, size, results);

void random_construction_bench(){
    std::cout << "Bench the random construction time of each data structure" << std::endl;

    std::vector<int> sizes = {50000, 100000, 500000, 1000000, 5000000, 10000000};

    for(auto size : sizes){
        std::stringstream name;
        name << "random-build-" << size;

        Results results;
        results.start(name.str());
        results.set_max(5);

        for(int i = 0; i < REPEAT; ++i){
            RANDOM_CONSTRUCTION(skiplist::SkipList, "skiplist", size);
            RANDOM_CONSTRUCTION(nbbst::NBBST, "nbbst", size);
            RANDOM_CONSTRUCTION(avltree::AVLTree, "avltree", size);
            //RANDOM_CONSTRUCTION(lfmst::MultiwaySearchTree, "lfmst", size);
            RANDOM_CONSTRUCTION(cbtree::CBTree, "cbtree", size);
        }

        results.finish();
    }
}

template<typename Tree, unsigned int Threads>
void seq_removal_bench(const std::string& name, unsigned int size, Results& results){
    Tree tree;

    for(unsigned int i = 0; i < size; ++i){
        tree.add(i);
    }
    
    unsigned int part = size / Threads;

    Clock::time_point t0 = Clock::now();

    std::vector<std::thread> pool;
    for(unsigned int tid = 0; tid < Threads; ++tid){
        pool.push_back(std::thread([&tree, part, size, tid](){
            thread_num = tid;

            for(unsigned int i = tid * part; i < (tid + 1) * part; ++i){
                tree.remove(i);
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});

    Clock::time_point t1 = Clock::now();

    std::cout << "Removal of " << name << " with " << size << " elements took " << get_duration(t0, t1) << " ms with " << Threads << " threads" << std::endl;
    results.add_result(name, get_duration(t0, t1));
}

#define SEQUENTIAL_REMOVAL(type, name, size)\
    seq_removal_bench<type<int, 1>, 1>(name, size, results);\
    seq_removal_bench<type<int, 2>, 2>(name, size, results);\
    seq_removal_bench<type<int, 3>, 3>(name, size, results);\
    seq_removal_bench<type<int, 4>, 4>(name, size, results);\
    seq_removal_bench<type<int, 8>, 8>(name, size, results);

void seq_removal_bench(){
    std::cout << "Bench the sequential removal time of each data structure" << std::endl;
    
    std::vector<int> small_sizes = {1000, 5000, 10000};
    
    for(auto size : small_sizes){
        std::stringstream name;
        name << "sequential-removal-" << size;

        Results results;
        results.start(name.str());
        results.set_max(5);
        
        for(int i = 0; i < REPEAT; ++i){
            SEQUENTIAL_REMOVAL(skiplist::SkipList, "skiplist", size);
            SEQUENTIAL_REMOVAL(nbbst::NBBST, "nbbst", size);
            SEQUENTIAL_REMOVAL(avltree::AVLTree, "avltree", size);
            //SEQUENTIAL_REMOVAL(lfmst::MultiwaySearchTree, "Multiway Search Tree", size);
            SEQUENTIAL_REMOVAL(cbtree::CBTree, "cbtree", size);
        }

        results.finish();
    }

    std::vector<int> sizes = {50000, 100000, 500000, 1000000, 5000000, 10000000};

    for(auto size : sizes){
        std::stringstream name;
        name << "sequential-removal-" << size;

        Results results;
        results.start(name.str());
        results.set_max(5);
        
        for(int i = 0; i < REPEAT; ++i){
            SEQUENTIAL_REMOVAL(skiplist::SkipList, "skiplist", size);
            //Too slow SEQUENTIAL_REMOVAL(nbbst::NBBST, "NBBST", size);
            SEQUENTIAL_REMOVAL(avltree::AVLTree, "avltree", size);
            //SEQUENTIAL_REMOVAL(lfmst::MultiwaySearchTree, "Multiway Search Tree", size);
            SEQUENTIAL_REMOVAL(cbtree::CBTree, "cbtree", size);
        }

        results.finish();
    }
}

template<typename Tree, unsigned int Threads>
void random_removal_bench(const std::string& name, unsigned int size, Results& results){
    Tree tree;

    std::vector<int> elements;
    for(unsigned int i = 0; i < size; ++i){
        elements.push_back(i);
    }

    random_shuffle(elements.begin(), elements.end());
    
    for(unsigned int i = 0; i < size; ++i){
        tree.add(elements[i]);
    }
    
    unsigned int part = size / Threads;

    Clock::time_point t0 = Clock::now();

    std::vector<std::thread> pool;
    for(unsigned int tid = 0; tid < Threads; ++tid){
        pool.push_back(std::thread([&tree, &elements, part, size, tid](){
            thread_num = tid;

            for(unsigned int i = tid * part; i < (tid + 1) * part; ++i){
                tree.remove(elements[i]);
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});

    Clock::time_point t1 = Clock::now();

    std::cout << "Removal of " << name << " with " << size << " elements took " << get_duration(t0, t1) << " ms with " << Threads << " threads" << std::endl;
    results.add_result(name, get_duration(t0, t1));
}

#define RANDOM_REMOVAL(type, name, size)\
    random_removal_bench<type<int, 1>, 1>(name, size, results);\
    random_removal_bench<type<int, 2>, 2>(name, size, results);\
    random_removal_bench<type<int, 3>, 3>(name, size, results);\
    random_removal_bench<type<int, 4>, 4>(name, size, results);\
    random_removal_bench<type<int, 8>, 8>(name, size, results);

void random_removal_bench(){
    std::cout << "Bench the random removal time of each data structure" << std::endl;

    std::vector<int> sizes = {50000, 100000, 500000, 1000000, 5000000, 10000000};

    for(auto size : sizes){
        std::stringstream name;
        name << "random-removal-" << size;

        Results results;
        results.start(name.str());
        results.set_max(5);

        for(int i = 0; i < REPEAT; ++i){
            RANDOM_REMOVAL(skiplist::SkipList, "skiplist", size);
            RANDOM_REMOVAL(nbbst::NBBST, "nbbst", size);
            RANDOM_REMOVAL(avltree::AVLTree, "avltree", size);
            //RANDOM_REMOVAL(lfmst::MultiwaySearchTree, "lfmst", size);
            RANDOM_REMOVAL(cbtree::CBTree, "cbtree", size);
        }

        results.finish();
    }
}

template<typename Tree, unsigned int Threads>
void search_bench(const std::string& name, unsigned int size, Tree& tree, Results& results){
    Clock::time_point t0 = Clock::now();

    std::vector<std::thread> pool;
    for(unsigned int tid = 0; tid < Threads; ++tid){
        pool.push_back(std::thread([&tree, size, tid](){
            thread_num = tid;
    
            std::mt19937_64 engine(time(0) + tid);
            std::uniform_int_distribution<int> distribution(0, size);

            for(int s = 0; s < SEARCH_BENCH_OPERATIONS; ++s){
                tree.contains(distribution(engine));
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});

    Clock::time_point t1 = Clock::now();
    
    milliseconds ms = std::chrono::duration_cast<milliseconds>(t1 - t0);
    unsigned long throughput = (Threads * SEARCH_BENCH_OPERATIONS) / ms.count();

    std::cout << name << "-" << size << " search througput with " << Threads << " threads = " << throughput << " operations / ms" << std::endl;
    results.add_result(name, throughput);
}

template<typename Tree>
void fill_random(Tree& tree, unsigned int size){
    std::vector<int> values;
    for(unsigned int i = 0; i < size; ++i){
        values.push_back(i);
    }

    random_shuffle(values.begin(), values.end());
    
    for(unsigned int i = 0; i < size; ++i){
        tree.add(values[i]);
    }
}

template<typename Tree>
void fill_sequential(Tree& tree, unsigned int size){
    for(unsigned int i = 0; i < size; ++i){
        tree.add(i);
    }
}

template<typename Tree, unsigned int Threads>
void search_random_bench(const std::string& name, unsigned int size, Results& results){
    Tree tree;
    
    fill_random(tree, size);
    
    search_bench<Tree, Threads>(name, size, tree, results);

    //Empty the tree
    for(unsigned int i = 0; i < size; ++i){
        tree.remove(i);
    }
}

#define SEARCH_RANDOM(type, name, size)\
    search_random_bench<type<int, 1>, 1>(name, size, results);\
    search_random_bench<type<int, 2>, 2>(name, size, results);\
    search_random_bench<type<int, 3>, 3>(name, size, results);\
    search_random_bench<type<int, 4>, 4>(name, size, results);\
    search_random_bench<type<int, 8>, 8>(name, size, results);

void search_random_bench(){
    std::cout << "Bench the search performances of each data structure with random insertion" << std::endl;

    std::vector<int> sizes = {50000, 100000, 500000, 1000000, 5000000, 10000000};

    for(auto size : sizes){
        std::stringstream name;
        name << "random-search-" << size;

        Results results;
        results.start(name.str());
        results.set_max(5);

        for(int i = 0; i < REPEAT; ++i){
            SEARCH_RANDOM(skiplist::SkipList, "skiplist", size);
            SEARCH_RANDOM(nbbst::NBBST, "nbbst", size);
            SEARCH_RANDOM(avltree::AVLTree, "avltree", size);
            //SEARCH_RANDOM(lfmst::MultiwaySearchTree, "lfmst", size);
            SEARCH_RANDOM(cbtree::CBTree, "cbtree", size);
        }

        results.finish();
    }
}

template<typename Tree, unsigned int Threads>
void search_sequential_bench(const std::string& name, unsigned int size, Results& results){
    Tree tree;
    
    fill_sequential(tree, size);
    
    search_bench<Tree, Threads>(name, size, tree, results);

    //Empty the tree
    for(unsigned int i = 0; i < size; ++i){
        tree.remove(i);
    }
}

#define SEARCH_SEQUENTIAL(type, name, size)\
    search_sequential_bench<type<int, 1>, 1>(name, size, results);\
    search_sequential_bench<type<int, 2>, 2>(name, size, results);\
    search_sequential_bench<type<int, 3>, 3>(name, size, results);\
    search_sequential_bench<type<int, 4>, 4>(name, size, results);\
    search_sequential_bench<type<int, 8>, 8>(name, size, results);

void search_sequential_bench(){
    std::cout << "Bench the search performances of each data structure with sequential insertion" << std::endl;
    
    std::vector<int> small_sizes = {1000, 5000, 10000};
    
    for(auto size : small_sizes){
        std::stringstream name;
        name << "sequential-search-" << size;

        Results results;
        results.start(name.str());
        results.set_max(5);

        for(int i = 0; i < REPEAT; ++i){
            SEARCH_SEQUENTIAL(skiplist::SkipList, "skiplist", size);
            SEARCH_SEQUENTIAL(nbbst::NBBST, "nbbst", size);
            SEARCH_SEQUENTIAL(avltree::AVLTree, "avltree", size);
            //SEARCH_SEQUENTIAL(lfmst::MultiwaySearchTree, "lfmst", size);
            SEARCH_SEQUENTIAL(cbtree::CBTree, "cbtree", size);
        }

        results.finish();
    }

    std::vector<int> sizes = {50000, 100000, 500000, 1000000, 5000000, 10000000};

    for(auto size : sizes){
        std::stringstream name;
        name << "sequential-search-" << size;

        Results results;
        results.start(name.str());
        results.set_max(5);

        for(int i = 0; i < REPEAT; ++i){
            SEARCH_SEQUENTIAL(skiplist::SkipList, "skiplist", size);
            //The nbbst is far too slow SEARCH_SEQUENTIAL(nbbst::NBBST, "nbbst", size);
            SEARCH_SEQUENTIAL(avltree::AVLTree, "avltree", size);
            //SEARCH_SEQUENTIAL(lfmst::MultiwaySearchTree, "lfmst", size);
            SEARCH_SEQUENTIAL(cbtree::CBTree, "cbtree", size);
        }

        results.finish();
    }
}

void bench(){
    std::cout << "Tests the performance of the different versions" << std::endl;

    //Launch the random benchmark
    //random_bench();
    //skewed_bench();

    //Launch the construction benchmark
    //seq_construction_bench();
    //random_construction_bench();
    
    //Launch the removal benchmark
    //random_removal_bench();
    seq_removal_bench();

    //Launch the search benchmark
    //search_random_bench();
    //search_sequential_bench();
}
