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
    const int key;
    long version;
    bool value;
    Node* parent;
    Node* left;
    Node* right;

    Node(int key) : key(key), parent(nullptr), left(nullptr), right(nullptr) {};
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
            left = child;
        }

        assert(direction != 0);
    }
};

//TODO Use different specialized enum
enum Result {
    FOUND,
    NOT_FOUND,
    RETRY
};

template<typename T, int Threads>
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
        Result attemptUpdate(Node* node);

        bool canUnlink(Node* n);
        void waitUntilNotChanging(Node* node);
        void fixHeightAndRebalance(Node* node);
        void rotateRight(Node* node);
};

template<typename T, int Threads>
AVLTree<T, Threads>::AVLTree(){
    rootHolder = new Node(INT_MIN);
    rootHolder->height = 1;
    rootHolder->version = 0;
    rootHolder->value = false;

    /*rootHolder->right = new Node(0);
    rootHolder->right->version = 0;
    rootHolder->right->value = false;
    rootHolder->right->parent = rootHolder;*/
}
        
template<typename T, int Threads>
bool AVLTree<T, Threads>::contains(T value){
    return attemptGet(hash(value), rootHolder, 1, 0) == FOUND;
}

template<typename T, int Threads>
Result AVLTree<T, Threads>::attemptGet(int key, Node* node, int dir, long nodeV){
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
            return child->value ? FOUND : NOT_FOUND;//Verify that it's a value node
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

template<typename T, int Threads>
bool AVLTree<T, Threads>::add(T value){
    return attemptPut(hash(value), rootHolder, 1, 0) == NOT_FOUND;//Check the return value
}

template<typename T, int Threads>
Result AVLTree<T, Threads>::attemptPut(int key, Node* node, int dir, long nodeV){
    Result p = RETRY;

    do {
        Node* child = node->child(dir);
        
        if(((node->version^nodeV) & IgnoreGrow) != 0){
            return RETRY;
        }

        if(!child){
            p = attemptInsert(key, node, dir, nodeV);
        } else {
            assert(child);

            int nextD = key - child->key;

            if(nextD == 0){
                p = attemptUpdate(child);
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

template<typename T, int Threads>
Result AVLTree<T, Threads>::attemptUpdate(Node* node){
   //synchronized(node){
     
     if(node->version == Unlinked){
        return RETRY;
     }

     //bool old = node->value;
     node->value = true;
     return NOT_FOUND;
     //return old ? FOUND : NOT_FOUND;
       
   //}
}

template<typename T, int Threads>
Result AVLTree<T, Threads>::attemptInsert(int key, Node* node, int dir, long nodeV){
    //synchronized(node){

        if(((node->version ^ nodeV) & IgnoreGrow) != 0 || node->child(dir)){
            return RETRY;
        }

        Node* newNode = new Node(1, key, 0, node, nullptr, nullptr);
        newNode->value = true;
        node->setChild(dir, newNode);
    //}

    fixHeightAndRebalance(node);

    return NOT_FOUND;
}

template<typename T, int Threads>
bool AVLTree<T, Threads>::remove(T value){
    return attemptRemove(hash(value), rootHolder, 1, 0) == FOUND;
}

template<typename T, int Threads>
Result AVLTree<T, Threads>::attemptRemove(int key, Node* node, int dir, long nodeV){
    //std::cout << "attempt remove " << std::endl;

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

                    p = attemptRemove(key, child, nextD, chV);
                }
            }
        }
    } while(p == RETRY);

    return p;
}

template<typename T, int Threads>
Result AVLTree<T, Threads>::attemptRmNode(Node* parent, Node * node){
    //std::cout << "attempt rm node" << std::endl;

    if(!node->value){
        return NOT_FOUND;
    }

    //bool prev;
    if(!canUnlink(node)){
        //synchronized(node){

        if(node->version == Unlinked || canUnlink(node)){
            return RETRY;
        }

        //prev = node->value;
        node->value = false;

        //}
    } else {
        //synchronized(parent){
            if(parent->version == Unlinked || node->parent != parent || node->version == Unlinked){
                //std::cout << "parent->version " << parent->version << std::endl;
                //std::cout << "node->version   " << node->version << std::endl;
                //std::cout << "node->parent    " << node->parent << std::endl;
                //std::cout << "parent          " << parent << std::endl;


                return RETRY;
            }

            //synchronized(node){
      //          prev = node->value;
                node->value = false;

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

    return FOUND;
    //return prev ? FOUND : NOT_FOUND;
}

template<typename T, int Threads>
bool AVLTree<T, Threads>::canUnlink(Node* n){
    return !n->left || !n->right;
}

template<typename T, int Threads>
void AVLTree<T, Threads>::waitUntilNotChanging(Node* node){
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

template<typename T, int Threads>
void AVLTree<T, Threads>::fixHeightAndRebalance(Node* /* node*/){
    //No rebalancing yet
}

int height(Node* node){
    return !node ? 0 : node->height;
}

template<typename T, int Threads>
void AVLTree<T, Threads>::rotateRight(Node* node){
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
