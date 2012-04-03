#include <vector>
#include <string>
#include <iostream>
#include <random>
#include <functional>
#include <climits> //TODO Remove that dependency
#include <thread>
#include <algorithm>

#include "test.hpp"
#include "HazardManager.hpp" //To manipulate thread_num

//Include all the trees implementations
#include "skiplist/SkipList.hpp"
#include "nbbst/NBBST.hpp"
#include "avltree/AVLTree.hpp"
#include "lfmst/MultiwaySearchTree.hpp"
#include "cbtree/CBTree.hpp"

//Number of nodes for the tests (for up to 32 threads)
#define N 10000//00         //A too big number can put nodes in swap

template<typename T>
void testST(const std::string& name){
    std::cout << "Test single-threaded (with " << N << " elements) " << name << std::endl;
    
    T tree;
    
    std::mt19937_64 engine(time(NULL));
    std::uniform_int_distribution<int> distribution(0, INT_MAX);
    auto generator = std::bind(distribution, engine);

    //Try remove numbers in the empty tree
    for(unsigned int i = 0; i < 1000; ++i){
        auto number = generator();

        assert(!tree.contains(number));
        assert(!tree.remove(number));
    }

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
        int number = generator();

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
        int number = generator();

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

//This test could be improved with some randomness
template<typename T, unsigned int Threads>
void testMT(){
    std::cout << "Test with " << Threads << " threads" << std::endl;

    T tree;

    std::vector<std::thread> pool;
    for(unsigned int i = 0; i < Threads; ++i){
        pool.push_back(std::thread([&tree, i](){
            thread_num = i;

            int tid = thread_num;

            //Insert sequential numbers
            for(int j = tid * N; j < (tid + 1) * N; ++j){
                assert(!tree.contains(j));
                assert(tree.add(j));
                assert(tree.contains(j));
            }

            //Remove all the sequential numbers
            for(int j = tid * N; j < (tid + 1) * N; ++j){
                assert(tree.contains(j));
                assert(tree.remove(j));   
                assert(!tree.contains(j));
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});

    pool.clear();
    
    for(unsigned int i = 0; i < Threads; ++i){
        pool.push_back(std::thread([&tree, i](){
            thread_num = i;

            //Verify that every numbers has been removed correctly
            for(unsigned int i = 0; i < Threads * N; ++i){
                assert(!tree.contains(i));
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});
    
    std::cout << "Test with " << Threads << " threads passed succesfully" << std::endl;
}

#define TEST(type, name) \
    testST<type<int, 1>>(name);\
    std::cout << "Test multi-threaded (with " << N << " elements) " << name << std::endl;\
    testMT<type<int, 2>, 2>();\
    testMT<type<int, 3>, 3>();\
    testMT<type<int, 4>, 4>();\
    testMT<type<int, 6>, 6>();\
    testMT<type<int, 8>, 8>();\
    testMT<type<int, 12>, 12>();\
    testMT<type<int, 16>, 16>();\
    testMT<type<int, 32>, 32>();

void test(){
    std::cout << "Tests the different versions" << std::endl;

    TEST(skiplist::SkipList, "SkipList")
    TEST(nbbst::NBBST, "Non-Blocking Binary Search Tree")
    //TEST(avltree::AVLTree, "Optimistic AVL Tree")
    //TEST(lfmst::MultiwaySearchTree, "Lock Free Multiway Search Tree");
    //TEST(cbtree::CBTree, "Counter Based Tree");
}

