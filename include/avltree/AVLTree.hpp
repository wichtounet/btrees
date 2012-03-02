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

    volatile Node* child(int direction){
        if(direction > 0){
            return right;
        } else if(direction < 0){
            return left;
        }

        assert(false);
    }
};

enum Result {
    FOUND,
    NOT_FOUND,
    RETRY
};

template<typename T>
class AVLTree {
    public:
        AVLTree();
        
        bool contains(T value);

    private:
        Node* rootHolder;

        Result attemptGet(int key, Node* node, int dir, long nodeV);

        void waitUntilNotChanging(Node* node);
};

template<typename T>
AVLTree<T>::AVLTree(){
    rootHolder = new Node();

    rootHolder->right = new Node();
}
        
template<typename T>        
bool AVLTree<T>::contains(T value){
    return attemptGet(hash(value), rootHolder, 1, 0) == FOUND;
}

template<typename T>
Result AVLTree<T>::attemptGet(int key, Node* node, int dir, long nodeV){
    while(true){
        Node* child = node->child(dir);

        if(((node->version ^ nodeV) & IgnoreGrow) != 0){
            return RETRY;
        }

        if(!child){
            return NOT_FOUND;
        }

        int nextD = key - child->key; //Compare to the other key

        if(nextD == 0){
            return FOUND;
        }

        long chV = child->version;

        if((chV & Shrinking) != 0){
            waitUntilNotChanging(child);
        } else if( chV != Unlinked && child == node->child(dir)){
            if(((node->version^nodeV) & IgnoreGrow) != 0){
                return RETRY;
            }

            Result result = attemptGet(key, child, nextD, chV);
            if(result != RETRY){
                return result;
            }

        }
    }
}

} //end of avltree

#endif
