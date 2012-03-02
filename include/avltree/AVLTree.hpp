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
static long ShrinkCountIncr = 1L << 11;
static long IgnoreGrow = ~(Growing | GrowCountMask);

static int SpinCount = 100;

struct Node {
    int height;
    int key;
    long version;
    Node* parent;
    Node* left;
    Node* right;

    Node(){};
    Node(int height, int key, long version, Node* parent, Node* left, Node* right) : height(height), key(key), version(version), parent(parent), left(left), right(right) {} 

    Node* child(int direction){
        if(direction > 0){
            return right;
        } else if(direction < 0){
            return left;
        }

        assert(false);
    }

    //Should only be called with lock on Node
    void setChild(int direction, Node* child){
        if(direction > 0){
            right = child;
        } else if(direction < 0){
            child = left;
        }

        assert(false);
    }
};

//TODO Use different specialized enum
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
        bool add(T value);
        bool remove(T value);

    private:
        Node* rootHolder;

        Result attemptGet(int key, Node* node, int dir, long nodeV);
        Result attemptPut(int key, Node* node, int dir, long nodeV);
        Result attemptInsert(int key, Node* node, int dir, long nodeV);
        Result attemptRemove(int key, Node* node, int dir, long nodeV);
        Result attemptRmNode(Node* parent, Node * node);

        bool canUnlink(Node* n);
        void waitUntilNotChanging(Node* node);
        void fixHeightAndRebalance(Node* node);
        void rotateRight(Node* node);
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

template<typename T>
bool AVLTree<T>::add(T value){
    return attemptPut(hash(value), rootHolder, 1, 0) == NOT_FOUND;//Check the return value
}

template<typename T>
Result AVLTree<T>::attemptPut(int key, Node* node, int dir, long nodeV){
    Result p = RETRY;

    do {
        Node* child = node->child(dir);
        
        if(((node->version^nodeV) & IgnoreGrow) != 0){
            return RETRY;
        }

        if(!child){
            p = attemptInsert(key, node, dir, nodeV);
        } else {
            int nextD = key - child->key;

            if(nextD == 0){
                //p = attemptUpdate(child);
                return NOT_FOUND;   //No update, the key is already inside the set
            } else {
                long chV = child->version;

                if((chV & Shrinking) != 0){
                    waitUntilNotChanging(child);
                } else if(chV != Unlinked && child == node->child(dir)){
                    if(((node->version ^ nodeV) & IgnoreGrow) != 0){
                        return RETRY;
                    }

                    p = attemptPut(key, child, nextD, chV);
                }
            }
        }
    } while(p == RETRY);

    return p;
}

template<typename T>
Result AVLTree<T>::attemptInsert(int key, Node* node, int dir, long nodeV){
    //synchronized(node){

        if(((node->version ^ nodeV) & IgnoreGrow) != 0 || node->child(dir) != nullptr){
            return RETRY;
        }

        node->setChild(dir, new Node(1, key, 0, node, nullptr, nullptr));

    //}

    fixHeightAndRebalance(node);

    return NOT_FOUND;
}

template<typename T>
bool AVLTree<T>::remove(T value){
    return attemptRemove(hash(value), rootHolder, 1, 0) == FOUND;
}

template<typename T>
Result AVLTree<T>::attemptRemove(int key, Node* node, int dir, long nodeV){
    Result p = RETRY;

    do {
        Node* child = node->child(dir);
        
        if(((node->version^nodeV) & IgnoreGrow) != 0){
            return RETRY;
        }

        if(!child){
            return NOT_FOUND;
        } else {
            int nextD = key - child->key;

            if(nextD == 0){
                p = attemptRmNode(node, child);
            } else {
                long chV = child->version;

                if((chV & Shrinking) != 0){
                    waitUntilNotChanging(child);
                } else if(chV != Unlinked && child == node->child(dir)){
                    if(((node->version ^ nodeV) & IgnoreGrow) != 0){
                        return RETRY;
                    }

                    p = attemptPut(key, child, nextD, chV);
                }
            }
        }
    } while(p == RETRY);

    return p;
}

template<typename T>
Result AVLTree<T>::attemptRmNode(Node* parent, Node * node){
    /*
    Verify
    if(!node->value){
        return null;
    }*/

    if(!canUnlink(node)){
        //synchronized(node){

        if(node->version == Unlinked || canUnlink(node)){
            return RETRY;
        }

        //prev = n.value
        //node->value = null;

        //}
    } else {
        //synchronized(parent){
            if(parent->version == Unlinked || node->parent != parent || node->version == Unlinked){
                return RETRY;
            }

            //synchronized(node){
                //prev = node->value;
                //node->value = null;

                if(canUnlink(node)){
                    Node* c = !node->left ? node->right : node->left;
                    
                    if(parent->left == node){
                        parent->left = c;
                    } else {
                        parent->right = c;
                    }

                    if(c){
                        c->parent = parent;
                    }

                    node->version = Unlinked;
                }
            //}
        //}
        fixHeightAndRebalance(parent);
    }

    //return prev;
}

template<typename T>
bool AVLTree<T>::canUnlink(Node* n){
    return !n->left || !n->right;
}

template<typename T>
void AVLTree<T>::waitUntilNotChanging(Node* node){
    long version = node->version;

    if((version & (Growing | Shrinking)) != 0){
        int i = 0;
        while(node->version == version && i < SpinCount){
            ++i;
        }

        if(i == SpinCount){
            //acquire lock on node
            //release lock on node
        }
    }
}

template<typename T>
void AVLTree<T>::fixHeightAndRebalance(Node* /* node*/){
    //No rebalancing yet
}

int height(Node* node){
    return !node ? 0 : node->height;
}

template<typename T>
void AVLTree<T>::rotateRight(Node* node){
    Node* nP = node->parent;
    Node* nL = node->left;
    Node* nLR = nL->right;
    
    node->version |= Shrinking;
    nL->version |= Growing;

    node->left = nLR;
    nL->right = node;

    if(nP->left == node){
        nP->left = nL;
    } else {
        nP->right = nL;
    }

    nL->parent = nP;
    node->parent = nL;

    if(nLR){
        nLR->parent = node;
    }

    int h = 1 + std::max(height(nLR), height(node->right));
    node->height = h;
    nL->height = 1 + std::max(height(nL->left), h);

    nL->version +=  GrowCountIncr;
    node->version += ShrinkCountIncr;
}

} //end of avltree

#endif