#include <malloc.h>
#include <pthread.h>
#include <unordered_map>

static void* (*old_malloc_hook)(size_t, const void*);
static void (*old_free_hook)(void*, const void*);

static unsigned long allocated = 0;

static bool end = false;
static std::unordered_map<void*, size_t> sizes;

static void* rqmalloc_hook(size_t size, const void* source);
static void rqfree_hook(void* memory, const void* source);

static void* rqmalloc_hook(size_t size, const void* /* source*/){
    void* result;
    
    __malloc_hook = old_malloc_hook;
    __free_hook = old_free_hook;
    
    result = malloc(size);

    old_malloc_hook = __malloc_hook;
    old_free_hook = __free_hook;

    if(!end){
        sizes[result] = size;
        allocated += size;
    }

    __malloc_hook = rqmalloc_hook;
    __free_hook = rqfree_hook;

    return result;
}

static void rqfree_hook(void* memory, const void* /* source */){
    __malloc_hook = old_malloc_hook;
    __free_hook = old_free_hook;
    
    free(memory);

    old_malloc_hook = __malloc_hook;
    old_free_hook = __free_hook;
    
    if(!end){
        allocated -= sizes[memory];
    }

    __malloc_hook = rqmalloc_hook;
    __free_hook = rqfree_hook;
}

void memory_init(){
    old_malloc_hook = __malloc_hook;
    old_free_hook = __free_hook;
    __malloc_hook = rqmalloc_hook;
    __free_hook = rqfree_hook;
    /*
    malloc_call     = __malloc_hook;
    __malloc_hook   = rqmalloc_hook;

    free_call       = __free_hook;
    __free_hook     = rqfree_hook;

    sizes.clear();*/
}

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <set>

#include "memory.hpp"
#include "Results.hpp"

//Include all the trees implementations
#include "skiplist/SkipList.hpp"
#include "nbbst/NBBST.hpp"
#include "avltree/AVLTree.hpp"
#include "lfmst/MultiwaySearchTree.hpp"
#include "cbtree/CBTree.hpp"

template<typename Tree>
void memory(const std::string& name, unsigned int size, Results& results){
    //Use random insertion in order to support non-balanced version
    std::vector<int> elements;
    for(unsigned int i = 0; i < size; ++i){
        elements.push_back(i);
    }

    random_shuffle(elements.begin(), elements.end());

    //For now on, count all the allocations
    allocated = 0;

    Tree* alloc_tree = new Tree();
    Tree& tree = *alloc_tree;

    //Fill the tree
    for(unsigned int i = 0; i < size; ++i){
        tree.add(elements[i]);
    }
    
    unsigned long usage = allocated;
    
    std::cout << name << "-" << size << " is using " << (usage / 1024) << " KB" << std::endl;
    results.add_result(name, (usage / 1024.0));
    
    //Empty the tree
    for(unsigned int i = 0; i < size; ++i){
        tree.remove(i);
    }

    delete alloc_tree;
}

template<typename Tree>
void memory_high(const std::string& name, unsigned int size, Results& results){
    std::mt19937_64 engine(time(0));

    std::uniform_int_distribution<int> valueDistribution(0, std::numeric_limits<int>::max());
    auto valueGenerator = std::bind(valueDistribution, engine);
    
    std::set<int> elements;
    std::vector<int> vector_elements;

    while(elements.size() < size){
        elements.insert(valueGenerator());
    }

    for(auto i : elements){
        vector_elements.push_back(i);
    }

    random_shuffle(vector_elements.begin(), vector_elements.end());

    //For now on, count all the allocations
    allocated = 0;

    Tree* alloc_tree = new Tree();
    Tree& tree = *alloc_tree;

    for(auto i : vector_elements){
        tree.add(i);
    }

    unsigned long usage = allocated;
    
    std::cout << name << "-" << size << " is using " << (usage / 1024) << " KB" << std::endl;
    results.add_result(name, (usage / 1024.0));
    
    //Empty the tree
    for(auto i : vector_elements){
        tree.remove(i);
    }

    delete alloc_tree;
}

void test_memory_consumption(){
    std::cout << "Test the memory consumption of each version" << std::endl;

    memory_init();

    thread_num = 0;

    std::vector<unsigned int> little_sizes = {1000, 10000, 100000};
    std::vector<unsigned int> big_sizes = {1000000, 10000000};
        
    Results results;
    results.start("memory-little");
    results.set_max(3);

    for(auto size : little_sizes){
        memory<skiplist::SkipList<int, 32>>("skiplist", size, results);
        memory<nbbst::NBBST<int, 32>>("nbbst", size, results);
        memory<lfmst::MultiwaySearchTree<int, 32>>("lfmst", size, results);
        memory<avltree::AVLTree<int, 32>>("avltree", size, results);
        memory<cbtree::CBTree<int, 32>>("cbtree", size, results);
    }

    results.finish();
    
    results.start("memory-little-high");
    results.set_max(3);

    for(auto size : little_sizes){
        memory_high<skiplist::SkipList<int, 32>>("skiplist", size, results);
        memory_high<nbbst::NBBST<int, 32>>("nbbst", size, results);
        memory_high<lfmst::MultiwaySearchTree<int, 32>>("lfmst", size, results);
        memory_high<avltree::AVLTree<int, 32>>("avltree", size, results);
        memory_high<cbtree::CBTree<int, 32>>("cbtree", size, results);
    }

    results.finish();

    return;
    
    results.start("memory-big");
    results.set_max(2);

    for(auto size : big_sizes){
        memory<skiplist::SkipList<int, 32>>("skiplist", size, results);
        memory<nbbst::NBBST<int, 32>>("nbbst", size, results);
        memory<lfmst::MultiwaySearchTree<int, 32>>("lfmst", size, results);
        memory<avltree::AVLTree<int, 32>>("avltree", size, results);
        memory<cbtree::CBTree<int, 32>>("cbtree", size, results);
    }

    results.finish();
    
    results.start("memory-big-high");
    results.set_max(2);

    for(auto size : big_sizes){
        memory_high<skiplist::SkipList<int, 32>>("skiplist", size, results);
        memory_high<nbbst::NBBST<int, 32>>("nbbst", size, results);
        memory_high<lfmst::MultiwaySearchTree<int, 32>>("lfmst", size, results);
        memory_high<avltree::AVLTree<int, 32>>("avltree", size, results);
        memory_high<cbtree::CBTree<int, 32>>("cbtree", size, results);
    }

    results.finish();
}

int main(){
    test_memory_consumption();

    end = true;

    return 0;
}
