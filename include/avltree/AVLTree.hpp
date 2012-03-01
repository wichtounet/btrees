#ifndef AVL_TREE_TREE
#define AVL_TREE_TREE

#include "hash.hpp"
#include "Utils.hpp"

namespace avltree {

static long Unlinked = 0x1L;
static long Growing = 0x2L;
static long GrowCountIncr = 1L << 3;
static long GrowCountMask = 0xFFL << 3;
static long Shrinking = 0x4L;
static long ShrkingCountIncr = 1L << 11;
static long IgnoreGrow = ~(Growing | GrowCountMask);

struct Node {
    volatile int height;
    volatile long version;
    volatile Node* parent;
    volatile Node* left;
    volatile Node* right;
    
    const int key;
};

template<typename T>
class AVLTree {
    public:
        AVLTree();

    private:
        Node* rootHolder;
};

template<typename T>
AVLTree<T>::AVLTree(){
    rootHolder = new Node();

    rootHolder->right = new Node();
}

} //end of avltree

#endif
