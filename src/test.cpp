#include <vector>
#include <string>
#include <iostream>
#include <random>
#include <functional>
#include <thread>
#include <algorithm>

#include "test.hpp"
#include "HazardManager.hpp" //To manipulate thread_num
#include "tree_type_traits.hpp"

//Include all the trees implementations
#include "skiplist/SkipList.hpp"
#include "nbbst/NBBST.hpp"
#include "avltree/AVLTree.hpp"
#include "lfmst/MultiwaySearchTree.hpp"
#include "cbtree/CBTree.hpp"

//Number of nodes inserted in single-threaded mode
#define ST_N 100000

//Number of nodes inserted in multi-threaded mode (for up to 32 threads)
#define MT_N 100000         //Warning: This number is inserted for each thread

//Utility macros to print debug messages during the tests
#define DEBUG_ENABLED true
#define DEBUG(message) if(DEBUG_ENABLED) std::cout << message << std::endl;

/*!
 * Launch a single threaded test on the given structure. 
 * \param T The type of the structure.
 * \param name The name of the structure being tested. 
 */
template<typename T>
void testST(const std::string& name){
    std::cout << "Test single-threaded (with " << ST_N << " elements) " << name << std::endl;

    thread_num = 0;
    
    T tree;
    
    std::mt19937_64 engine(time(NULL));

    //Note: The max() value cannot be handled by all data structure
    std::uniform_int_distribution<int> distribution(0, std::numeric_limits<int>::max() - 1);
    auto generator = std::bind(distribution, engine);

    DEBUG("Remove numbers in the empty tree");

    for(unsigned int i = 0; i < ST_N; ++i){
        auto number = generator();

        assert(!tree.contains(number));
        assert(!tree.remove(number));
    }
    
    DEBUG("Insert sequential numbers");

    unsigned int sequential_nodes = ST_N;

    if(!is_balanced<T>()){
        sequential_nodes /= 100;
    }

    for(unsigned int i = 0; i < sequential_nodes; ++i){
        assert(!tree.contains(i));
        assert(tree.add(i));
        assert(tree.contains(i));
    }
    
    DEBUG("Remove all the sequential numbers");
    
    for(unsigned int i = 0; i < sequential_nodes; ++i){
        assert(tree.contains(i));
        assert(tree.remove(i));     
        assert(!tree.contains(i));
    }
    
    DEBUG("Verify that the tree is empty");
    
    for(unsigned int i = 0; i < sequential_nodes; ++i){
        assert(!tree.contains(i));
    }

    std::vector<int> rand;
    
    DEBUG("Insert N random numbers in the tree");

    for(unsigned int i = 0; i < ST_N; ++i){
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
    
    DEBUG("Remove numbers not present in the tree");
    
    for(unsigned int i = 0; i < ST_N; ++i){
        int number = generator();

        if(!tree.contains(number)){
            assert(!tree.remove(number));
            assert(!tree.contains(number));
        }
    }
    
    DEBUG("Remove all the numbers in random order");

    random_shuffle(rand.begin(), rand.end());
    for(int number : rand){
        assert(tree.contains(number));
        assert(tree.remove(number));
    }
    
    DEBUG("Remove numbers in the empty tree");
    
    for(unsigned int i = 0; i < ST_N; ++i){
        auto number = generator();

        assert(!tree.contains(number));
        assert(!tree.remove(number));
    }

    std::cout << "Test passed successfully" << std::endl;
}

/*!
 * Launch the multithreaded tests on the given tree type. 
 * \param T The type of tree to test.
 * \param Threads The number of threads. 
 */
template<typename T, unsigned int Threads>
void testMT(){
    T tree;

    DEBUG("Insert and remove sequential numbers from the tree")

    int sequential_nodes = MT_N;

    if(!is_balanced<T>()){
        sequential_nodes /= 100;
    }

    std::vector<std::thread> pool;
    for(unsigned int i = 0; i < Threads; ++i){
        pool.push_back(std::thread([sequential_nodes, &tree, i](){
            thread_num = i;

            int tid = thread_num;

            //Insert sequential numbers
            for(int j = tid * sequential_nodes; j < (tid + 1) * sequential_nodes; ++j){
                assert(!tree.contains(j));
                assert(tree.add(j));
                assert(tree.contains(j));
            }

            //Remove all the sequential numbers
            for(int j = tid * sequential_nodes; j < (tid + 1) * sequential_nodes; ++j){
                assert(tree.contains(j));
                assert(tree.remove(j));   
                assert(!tree.contains(j));
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});
    pool.clear();
    
    DEBUG("Verify that all the numbers have been removed correctly")
    
    for(unsigned int i = 0; i < Threads; ++i){
        pool.push_back(std::thread([sequential_nodes, &tree, i](){
            thread_num = i;

            //Verify that every numbers has been removed correctly
            for(unsigned int i = 0; i < Threads * sequential_nodes; ++i){
                assert(!tree.contains(i));
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});
    pool.clear();

    std::vector<unsigned int> fixed_points;
    
    std::mt19937_64 fixed_engine(time(NULL));
    std::uniform_int_distribution<int> fixed_distribution(0, std::numeric_limits<int>::max() - 1);
    
    DEBUG("Compute the fixed points")

    while(fixed_points.size() < Threads){
        auto value = fixed_distribution(fixed_engine);
        
        if(std::find(fixed_points.begin(), fixed_points.end(), value) == fixed_points.end()){
            fixed_points.push_back(value);
            
            assert(tree.add(value));
        }
    }
    
    DEBUG("Make some operations by ensuring that the fixed points are not modified")

    for(unsigned int i = 0; i < Threads; ++i){
        pool.push_back(std::thread([&tree, &fixed_points, i](){
            thread_num = i;

            std::vector<int> rand;
            
            std::mt19937_64 engine(time(0) + i);

            //Note: The max() value cannot be handled by all data structure
            std::uniform_int_distribution<int> distribution(0, std::numeric_limits<int>::max() - 1);
            auto generator = std::bind(distribution, engine);

            std::uniform_int_distribution<int> operationDistribution(0, 99);
            auto operationGenerator = std::bind(operationDistribution, engine);

            for(int n = 0; n < 10000; ++n){
                auto value = generator();

                if(operationGenerator() < 33){
                    if(std::find(fixed_points.begin(), fixed_points.end(), value) == fixed_points.end()){
                        tree.remove(value);
                    }
                } else {
                    tree.add(value);

                    if(std::find(fixed_points.begin(), fixed_points.end(), value) == fixed_points.end()){
                        rand.push_back(value);
                    }
                }

                assert(tree.contains(fixed_points[i]));
            }

            for(auto& value : rand){
                tree.remove(value);
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});

    for_each(fixed_points.begin(), fixed_points.end(), [&tree](int value){tree.remove(value);});
    
    std::cout << "Test with " << Threads << " threads passed succesfully" << std::endl;
}

/*!
 * Launch all the tests on the given type.
 * \param type The type of the tree. 
 * \param the The name of the tree. 
 */
#define TEST(type, name) \
    std::cout << "Test with 1 threads" << std::endl;\
    testST<type<int, 1>>(name);\
    std::cout << "Test multi-threaded (with " << MT_N << " elements) " << name << std::endl;\
    testMT<type<int, 2>, 2>();\
    testMT<type<int, 3>, 3>();\
    testMT<type<int, 4>, 4>();\
    testMT<type<int, 6>, 6>();\
    testMT<type<int, 8>, 8>();\
    testMT<type<int, 12>, 12>();\
    testMT<type<int, 16>, 16>();\
    testMT<type<int, 32>, 32>();

/*!
 * Test all the different versions.
 */
void test(){
    std::cout << "Tests the different versions" << std::endl;

    //TEST(skiplist::SkipList, "SkipList")
    TEST(nbbst::NBBST, "Non-Blocking Binary Search Tree")
    //TEST(avltree::AVLTree, "Optimistic AVL Tree")
    //TEST(lfmst::MultiwaySearchTree, "Lock Free Multiway Search Tree");
    //TEST(cbtree::CBTree, "Counter Based Tree");
}
