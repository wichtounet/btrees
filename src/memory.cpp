#include <malloc.h>

static void my_init_hook(void);
static void* my_malloc_hook(size_t, const void*);

void (*__malloc_initialize_hook) (void) = my_init_hook;

static void* (*old_malloc_hook)(size_t, const void*);

static void my_init_hook(void){
    old_malloc_hook = __malloc_hook;
    __malloc_hook = my_malloc_hook;
}

static unsigned long allocated = 0;

static void * my_malloc_hook (size_t size, const void*){
    void *result;
    /* Restore all old hooks */
    __malloc_hook = old_malloc_hook;
    /* Call recursively */
    result = malloc (size);
    /* Save underlying hooks */
    old_malloc_hook = __malloc_hook;
    //Inc the allocated size
    allocated += size;
    /* printf might call malloc, so protect it too. */
    if(allocated % 1000000000 == 0){
        printf ("allocated=%lu\n", allocated);
    }
    /* Restore our own hooks */
    __malloc_hook = my_malloc_hook;
    return result;
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

unsigned int memory_kb(double size){
    return size / 1024.0;
}

std::string memory(double size){
    std::stringstream stream;
    
    if(size > (1024.0 * 1024.0 * 1024.0)){
        stream << (size / (1024.0 * 1024.0 * 1024.0)) << "GB";
    } else if(size > (1024.0 * 1024.0)){
        stream << (size / (1024.0 * 1024.0)) << "MB";
    } else if(size > 1024.0){
        stream << (size / 1024.0) << "KB";
    } else {
        stream << size << "B";
    }

    return stream.str();
}

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
    while(elements.size() < size){
        elements.insert(valueGenerator());
    }

    //For now on, count all the allocations
    allocated = 0;

    Tree* alloc_tree = new Tree();
    Tree& tree = *alloc_tree;

    //Fill the tree
    for(auto i : elements){
        tree.add(i);
    }

    unsigned long usage = allocated;
    
    std::cout << name << "-" << size << " is using " << (usage / 1024) << " KB" << std::endl;
    results.add_result(name, (usage / 1024.0));
    
    //Empty the tree
    for(auto i : elements){
        tree.remove(i);
    }

    delete alloc_tree;
}

void test_memory_consumption(){
    std::cout << "Test the memory consumption of each version" << std::endl;

    std::vector<unsigned int> little_sizes = {1000, 10000, 100000};
    std::vector<unsigned int> big_sizes = {1000000, 10000000};
        
    Results results;
    results.start("memory-little");

    for(auto size : little_sizes){
        memory<skiplist::SkipList<int, 32>>("skiplist", size, results);
        memory<nbbst::NBBST<int, 32>>("nbbst", size, results);
        //LFMST
        memory<avltree::AVLTree<int, 32>>("avltree", size, results);
        memory<cbtree::CBTree<int, 32>>("cbtree", size, results);
    }

    results.finish();
    
    results.start("memory-little-high");

    for(auto size : little_sizes){
        memory_high<skiplist::SkipList<int, 32>>("skiplist", size, results);
        memory_high<nbbst::NBBST<int, 32>>("nbbst", size, results);
        //LFMST
        memory_high<avltree::AVLTree<int, 32>>("avltree", size, results);
        memory_high<cbtree::CBTree<int, 32>>("cbtree", size, results);
    }

    results.finish();
    
    results.start("memory-big");

    for(auto size : big_sizes){
        memory<skiplist::SkipList<int, 32>>("skiplist", size, results);
        memory<nbbst::NBBST<int, 32>>("nbbst", size, results);
        //LFMST
        memory<avltree::AVLTree<int, 32>>("avltree", size, results);
        memory<cbtree::CBTree<int, 32>>("cbtree", size, results);
    }

    results.finish();
    
    results.start("memory-big-high");

    for(auto size : big_sizes){
        memory_high<skiplist::SkipList<int, 32>>("skiplist", size, results);
        memory_high<nbbst::NBBST<int, 32>>("nbbst", size, results);
        //LFMST
        memory_high<avltree::AVLTree<int, 32>>("avltree", size, results);
        memory_high<cbtree::CBTree<int, 32>>("cbtree", size, results);
    }

    results.finish();
}

