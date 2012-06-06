#ifndef TREE_TYPE_TRAITS
#define TREE_TYPE_TRAITS

#include "nbbst/NBBST.hpp"
#include "lfmst/MultiwaySearchTree.hpp"

template<typename Tree>
struct tree_type_traits {
    static const bool balanced = true;
};

template<typename T, int Threads>
struct tree_type_traits<nbbst::NBBST<T, Threads>> {
    static const bool balanced = false;
};

template<typename Tree>
bool is_balanced(){
    return tree_type_traits<Tree>::balanced;
}

#endif
