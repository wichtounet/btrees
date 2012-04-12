#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <set>

//For reading memory usage
#include <stdio.h>
#include <proc/readproc.h>

#include "memory.hpp"

//Include all the trees implementations
#include "skiplist/SkipList.hpp"
#include "nbbst/NBBST.hpp"
#include "avltree/AVLTree.hpp"
#include "lfmst/MultiwaySearchTree.hpp"
#include "cbtree/CBTree.hpp"

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
void memory(const std::string& name, int size){
    //Use random insertion in order to support non-balanced version
    std::vector<int> elements;
    for(int i = 0; i < size; ++i){
        elements.push_back(i);
    }

    random_shuffle(elements.begin(), elements.end());

    struct proc_t usage1;
    struct proc_t usage2;

    //Lookup for usage
    look_up_our_self(&usage1);

    Tree tree;

    //Fill the tree
    for(int i = 0; i < size; ++i){
        tree.add(elements[i]);
    }
    
    //Lookup for usage
    look_up_our_self(&usage2);
    
    std::cout << name << "-" << size << " is using " << memory(usage2.vsize - usage1.vsize)  << std::endl;
    
    //Empty the tree
    for(int i = 0; i < size; ++i){
        tree.remove(i);
    }
}

template<typename Tree>
void memory_high(const std::string& name, unsigned int size){
    std::mt19937_64 engine(time(0));

    std::uniform_int_distribution<int> valueDistribution(0, std::numeric_limits<int>::max());
    auto valueGenerator = std::bind(valueDistribution, engine);
    
    std::set<int> elements;
    while(elements.size() < size){
        elements.insert(valueGenerator());
    }

    struct proc_t usage1;
    struct proc_t usage2;

    //Lookup for usage
    look_up_our_self(&usage1);

    Tree tree;

    //Fill the tree
    for(auto i : elements){
        tree.add(i);
    }
    
    //Lookup for usage
    look_up_our_self(&usage2);
    
    std::cout << name << "-" << size << " is using " << memory(usage2.vsize - usage1.vsize)  << std::endl;
    
    //Empty the tree
    for(auto i : elements){
        tree.remove(i);
    }
}

void test_memory_consumption(){
    std::cout << "Test the memory consumption of each version" << std::endl;

    std::vector<int> sizes = {1000, 10000, 100000, 1000000, 10000000};

    for(auto size : sizes){
        //memory_high<skiplist::SkipList<int, 32>>("SkipList", size);
        //memory_high<nbbst::NBBST<int, 32>>("NBBST", size);
    }
    
    for(auto size : sizes){
        //memory<skiplist::SkipList<int, 32>>("SkipList", size);
        //memory<nbbst::NBBST<int, 32>>("NBBST", size);
    }
}

