#ifndef AVL_TREE_TREE
#define AVL_TREE_TREE

#include "hash.hpp"

#include <mutex>

namespace avltree {

typedef std::lock_guard<std::mutex> scoped_lock;

/*static long Unlinked = 0x1L;
static long Growing = 0x2L;
static long GrowCountIncr = 1L << 3;
static long GrowCountMask = 0xFFL << 3;
static long Shrinking = 0x4L;
static long ShrinkCountIncr = 1L << 11;
static long IgnoreGrow = ~(Growing | GrowCountMask);*/

static int SpinCount = 100;

long beginChange(long ovl) { return ovl | 1; }
long endChange(long ovl) { return (ovl | 3) + 1; }
long UnlinkedOVL = 2;

bool isShrinking(long ovl) { return (ovl & 1) != 0; }
bool isUnlinked(long ovl) { return (ovl & 2) != 0; }
bool isShrinkingOrUnlinked(long ovl) { return (ovl & 3) != 0L; }

static const int UnlinkRequired = -1;
static const int RebalanceRequired = -2;
static const int NothingRequired = -3;

struct Node {
    int height;
    const int key;
    long version;
    bool value;
    Node* parent;
    Node* left;
    Node* right;

    std::mutex lock;

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

        /*Result attemptGet(int key, Node* node, int dir, long nodeV);
        Result attemptPut(int key, Node* node, int dir, long nodeV);
        Result attemptInsert(int key, Node* node, int dir, long nodeV);
        Result attemptRemove(int key, Node* node, int dir, long nodeV);
        Result attemptRmNode(Node* parent, Node * node);
        Result attemptUpdate(Node* node);
        bool canUnlink(Node* n);
        */
        
        Result attemptGet(int key, Node* node, int dir, long nodeV);

        /* Update stuff  */
        Result update(int key, int func, bool expected, bool newValue);
        Result updateUnderRoot(int key, int func, bool expected, bool newValue, Node* holder);
        bool attemptInsertIntoEmpty(int key, bool value, Node* holder);
        Result attemptUpdate(int key, int func, bool expected, bool newValue, Node* parent, Node* node, long nodeOVL);
        Result attemptNodeUpdate(int func, bool expected, bool newValue, Node* parent, Node* node);

        bool attemptUnlink_nl(Node* parent, Node* node);
        void waitUntilNotChanging(Node* node);
        
        /* Rebalancing stuff  */
        Node* attemptFixHeight_nl(Node* node);
        void fixHeightAndRebalance(Node* node);
        Node* attemptFixOrRebalance_nl(Node* nParent, Node* n);
        bool attemptBalanceLeft_nl(Node* nParent, Node* n, Node* nR, int hL0);
        bool attemptBalanceRight_nl(Node* nParent, Node* n, Node* nL, int hR0);

        /* Rotation stuff */        
        void rotateRight(Node* nParent, Node* n, Node* nL, int hR, Node* nLR, int hLR);
        void rotateLeft(Node* nParent, Node* n, int hL, Node* nR, Node* nRL, int hRL);
        void rotateRightOverLeft(Node* nParent, Node* n, Node* nL, int hR, Node* nLR);
        void rotateLeftOverRight(Node* nParent, Node* n, int hL, Node* nR, Node* nRL);
};

bool fixHeight(Node* n);
Node* fixHeight_nl(Node* n);

template<typename T, int Threads>
AVLTree<T, Threads>::AVLTree(){
    rootHolder = new Node(INT_MIN);
    rootHolder->height = 1;
    rootHolder->version = 0;
    rootHolder->value = false;
}
        
template<typename T, int Threads>
bool AVLTree<T, Threads>::contains(T value){
    int key = hash(value);

    while(true){
        Node* right = rootHolder->right;

        if(!right){
            return false;
        } else {
            int rightCmp = key - right->key;
            if(rightCmp == 0){
                return right->value;
            }

            int ovl = right->version;
            if(isShrinkingOrUnlinked(ovl)){
                waitUntilNotChanging(right);
            } else if(right == rootHolder->right){
                Result vo = attemptGet(key, right, rightCmp, ovl);
                if(vo != RETRY){
                    return vo == FOUND ? true : false;
                }
            }
        }
    }
}

template<typename T, int Threads>
Result AVLTree<T, Threads>::attemptGet(int key, Node* node, int dir, long nodeV){
    while(true){
        Node* child = node->child(dir);

        if(!child){
            if(node->version != nodeV){
                return RETRY;
            }

            return NOT_FOUND;
        } else {
            int childCmp = key - child->key;
            if(childCmp == 0){
                return child->value ? FOUND : NOT_FOUND;//Verify that it's a value node
            }

            long childOVL = child->version;
            if(isShrinkingOrUnlinked(childOVL)){
                waitUntilNotChanging(child);

                if(node->version != nodeV){
                    return RETRY;
                }
            } else if(child != node->child(dir)){
                if(node->version != nodeV){
                    return RETRY;
                }
            } else {
                if(node->version != nodeV){
                    return RETRY;
                }

                Result result = attemptGet(key, child, childCmp, childOVL);
                if(result != RETRY){
                    return result;
                }
            }
        }
    }
}

static const int UpdateAlways = 0;
static const int UpdateIfAbsent = 1;
static const int UpdateIfPresent = 2;
//static const int UpdateIfEq = 3; //Used for replacement

bool shouldUpdate(int func, bool prev, bool expected){
    switch(func){
        case UpdateAlways: return true;
        case UpdateIfAbsent: return !prev;
        case UpdateIfPresent: return prev;
    }
}

Result updateResult(int func, bool prev){
    return func == UpdateIfAbsent ? NOT_FOUND : FOUND;
}

Result noUpdateResult(int func, bool prev){
    return func == UpdateIfAbsent ? FOUND : NOT_FOUND;
}

template<typename T, int Threads>
bool AVLTree<T, Threads>::add(T value){
    //return attemptPut(hash(value), rootHolder, 1, 0) == NOT_FOUND;//Check the return value

    return update(hash(value), UpdateIfAbsent, false, true) == NOT_FOUND;
}

template<typename T, int Threads>
bool AVLTree<T, Threads>::remove(T value){
    //return attemptRemove(hash(value), rootHolder, 1, 0) == FOUND;

    return update(hash(value), UpdateIfPresent, true, false) == FOUND;
}

template<typename T, int Threads>
Result AVLTree<T, Threads>::update(int key, int func, bool expected, bool newValue){
    return updateUnderRoot(key, func, expected, newValue, rootHolder);
}

template<typename T, int Threads>
Result AVLTree<T, Threads>::updateUnderRoot(int key, int func, bool expected, bool newValue, Node* holder){
    while(true){
        Node* right = holder->right;

        if(!right){
            if(!shouldUpdate(func, false, expected)){
                return noUpdateResult(func, false);
            }

            if(!newValue || attemptInsertIntoEmpty(key, newValue, holder)){
                return updateResult(func, false);
            }
        } else {
            long ovl = right->version;

            if(isShrinkingOrUnlinked(ovl)){
                waitUntilNotChanging(right);
            } else if(right == holder->right){
                Result vo = attemptUpdate(key, func, expected, newValue, holder, right, ovl);
                if(vo != RETRY){
                    return vo;   
                }
            }
        }
    }
}

template<typename T, int Threads>
bool AVLTree<T, Threads>::attemptInsertIntoEmpty(int key, bool value, Node* holder){
    assert(value);//TODO Remove that later

    scoped_lock lock(holder->lock);

    if(!holder->right){
        holder->right = new Node(1, key, 0, holder, nullptr, nullptr);
        holder->right->value = value;
        holder->height = 2;
        return true;
    } else {
        return false;
    }
}

template<typename T, int Threads>
Result AVLTree<T, Threads>::attemptUpdate(int key, int func, bool expected, bool newValue, Node* parent, Node* node, long nodeOVL){
    assert(nodeOVL != UnlinkedOVL);

    int cmp = key - node->key;
    if(cmp == 0){
        return attemptNodeUpdate(func, expected, newValue, parent, node);
    }

    while(true){
        Node* child = node->child(cmp);

        if(node->version != nodeOVL){
            return RETRY;
        }

        if(!child){
            if(!newValue){
                return NOT_FOUND;
            } else {
                bool success;
                Node* damaged;

                {
                    scoped_lock lock(node->lock);    
                
                    if(node->version != nodeOVL){
                        return RETRY;
                    }

                    if(node->child(cmp)){
                        success = false;
                        damaged = nullptr;
                    } else {
                        if(!shouldUpdate(func, false, expected)){
                            return noUpdateResult(func, false);
                        }

                        Node* newNode = new Node(1, key, 0, node, nullptr, nullptr);
                        newNode->value = true;
                        node->setChild(cmp, newNode);

                        success = true;

                        damaged = fixHeight_nl(node);
                    }
                }

                if(success){
                    //fixHeightAndRebalance(damaged);
                    return updateResult(func, nullptr);
                }
            }
        } else {
            long childOVL = child->version;

            if(isShrinkingOrUnlinked(childOVL)){
                waitUntilNotChanging(child);
            } else if(child != node->child(cmp)){
                //RETRY
            } else {
                if(node->version != nodeOVL){
                    return RETRY;
                }

                Result vo = attemptUpdate(key, func, expected, newValue, node, child, childOVL);
                if(vo != RETRY){
                    return vo;
                }
            }
        }
    }
}

template<typename T, int Threads>
Result AVLTree<T, Threads>::attemptNodeUpdate(int func, bool expected, bool newValue, Node* parent, Node* node){
    if(!newValue){
        if(!node->value){
            //TODO CHECK
            return NOT_FOUND;
        }
    }

    if(!newValue && (!node->left || !node->right)){
        bool prev;
        Node* damaged;

        {
            scoped_lock parentLock(parent->lock);
            
            if(isUnlinked(parent->version) || node->parent != parent){
                return RETRY;
            }

            {
                scoped_lock lock(node->lock);
                
                prev = node->value;

                if(!shouldUpdate(func, prev, expected)){
                    return noUpdateResult(func, prev);
                }

                if(!prev){
                    return updateResult(func, prev);
                }

                if(!attemptUnlink_nl(parent, node)){
                    return RETRY;
                }
            }

            damaged = fixHeight_nl(parent);
        }

        //fixHeightAndRebalance(damaged);

        return updateResult(func, prev);
    } else {
        scoped_lock lock(node->lock);

        if(isUnlinked(node->version)){
            return RETRY;
        }

        bool prev = node->value;
        if(!shouldUpdate(func, prev, expected)){
            return noUpdateResult(func, prev);
        }

        if(!newValue && (!node->left || !node->right)){
            return RETRY;
        }

        node->value = newValue;
        return updateResult(func, prev);
    }
}




/*template<typename T, int Threads>
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
    scoped_lock lock(node->lock);

    if(node->version == Unlinked){
        return RETRY;
    }

    node->value = true;

    return NOT_FOUND;
}

template<typename T, int Threads>
Result AVLTree<T, Threads>::attemptInsert(int key, Node* node, int dir, long nodeV){
    node->lock.lock();

    if(((node->version ^ nodeV) & IgnoreGrow) != 0 || node->child(dir)){
        node->lock.unlock();

        return RETRY;
    }

    Node* newNode = new Node(1, key, 0, node, nullptr, nullptr);
    newNode->value = true;
    node->setChild(dir, newNode);

    node->lock.unlock();

    fixHeightAndRebalance(node);

    return NOT_FOUND;
}*/

/*template<typename T, int Threads>
Result AVLTree<T, Threads>::attemptRemove(int key, Node* node, int dir, long nodeV){
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
    if(!node->value){
        return NOT_FOUND;
    }

    //bool prev;
    if(!canUnlink(node)){
        scoped_lock lock(node->lock); 

        if(node->version == Unlinked || canUnlink(node)){
            return RETRY;
        }

        //prev = node->value;
        node->value = false;
    } else {
        parent->lock.lock();

        if(parent->version == Unlinked || node->parent != parent || node->version == Unlinked){
            parent->lock.unlock();

            return RETRY;
        }

        node->lock.lock();

        //prev = node->value;
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

        node->lock.unlock();
        parent->lock.unlock();

        fixHeightAndRebalance(parent);
    }

    return FOUND;
    //return prev ? FOUND : NOT_FOUND;
}*/

/*template<typename T, int Threads>
bool AVLTree<T, Threads>::canUnlink(Node* n){
    return !n->left || !n->right;
}*/

template<typename T, int Threads>
void AVLTree<T, Threads>::waitUntilNotChanging(Node* node){
    long version = node->version;

    if(isShrinking(version)){
        for(int i = 0; i < SpinCount; ++i){
            if(version != node->version){
                return;
            }
        }
            
        node->lock.lock();
        node->lock.unlock();
    }
}

int height(Node* node){
    return !node ? 0 : node->height;
}

int nodeCondition(Node* node){
    Node* nL = node->left;
    Node* nR = node->right;

    // unlink is required
    if((!nL || !nR) && !node->value){
        return UnlinkRequired;
    }

    int hN = node->height;
    int hL0 = height(nL);
    int hR0 = height(nR);
    int hNRepl = 1 + std::max(hL0, hR0);
    int bal = hL0 - hR0;

    // rebalance is required ?
    if(bal < -1 || bal > 1){
        return RebalanceRequired;
    }

    return hN != hNRepl ? hNRepl : NothingRequired;
}

template<typename T, int Threads>
void AVLTree<T, Threads>::fixHeightAndRebalance(Node* node){
    while(node->parent){
        int condition = nodeCondition(node);
        if(condition == NothingRequired || isUnlinked(node->version)){
            //Nothing to do or not point in fixing this node
            return;
        }

        Node* next;
        if(condition != UnlinkRequired && condition != RebalanceRequired){
            scoped_lock lock(node->lock);

            next = attemptFixHeight_nl(node);
        } else {
            Node* nParent = node->parent;

            scoped_lock lock(nParent->lock);
            if(nParent != node->parent){
                next = nullptr; //Retry
            } else {
                scoped_lock lock(node->lock);
                next = attemptFixOrRebalance_nl(nParent, node);
            }
        }

        if(next){
            node = next;
        }
        //else retry
    }
}

template<typename T, int Threads>
Node* AVLTree<T, Threads>::attemptFixHeight_nl(Node* node){
    int c = nodeCondition(node);

    switch(c){
        case RebalanceRequired:
        case UnlinkRequired:
            return nullptr;
        case NothingRequired:
            return rootHolder;
        default:
            node->height = c;
            return node->parent;
    }
}

bool fixHeight(Node* n){
    int hL = height(n->left);
    int hR = height(n->right);
    int hRepl = 1 + std::max(hL, hR);

    if(hRepl != n->height){
        n->height = hRepl;
        return true;
    } else {
        return false;
    }
}

Node* fixHeight_nl(Node* node){
    int c = nodeCondition(node);

    switch(c){
        case RebalanceRequired:
        case UnlinkRequired:
            return node;
        case NothingRequired:
            return nullptr;
        default:
            node->height = c;
            return node->parent;
    }
}
        
template<typename T, int Threads>
Node* AVLTree<T, Threads>::attemptFixOrRebalance_nl(Node* nParent, Node* n){
    Node* nL = n->left;
    Node* nR = n->right;

    if((!nL || !nR) && !n->value){
        return attemptUnlink_nl(nParent, n) ? nParent : nullptr;
    }
    
    int hN = n->height;
    int hL0 = height(nL);
    int hR0 = height(nR);
    int hNRepl = 1 + std::max(hL0, hR0);
    int bal = hL0 - hR0;

    if(bal > 1){
        if(!attemptBalanceRight_nl(nParent, n, nL, hR0)){
            return nullptr;
        }
    } else if(bal < -1){
        if(!attemptBalanceLeft_nl(nParent, n, nR, hL0)){
            return nullptr;
        }
    } else if(hNRepl != hN) {
        n->height = hNRepl;
    } else {
        return rootHolder;
    }

    return fixHeight(nParent) ? nParent : rootHolder;
}

template<typename T, int Threads>
bool AVLTree<T, Threads>::attemptBalanceRight_nl(Node* nParent, Node* n, Node* nL, int hR0){
    scoped_lock lock(nL->lock);

    int hL = nL->height;
    if(hL - hR0 <= 1){
        return false;
    } else {
        Node* nLR = nL->right;
        int hLR0 = height(nLR);

        if(height(nL->left) >= hLR0){
            rotateRight(nParent, n, nL, hR0, nLR, hLR0);
        } else {
            scoped_lock lock(nLR->lock);

            int hLR = nLR->height;
            if(height(nL->left) >= hLR){
                rotateRight(nParent, n, nL, hR0, nLR, hLR);
            } else {
                rotateRightOverLeft(nParent, n, nL, hR0, nLR);
            }
        }

        return true;
    }
}

template<typename T, int Threads>
bool AVLTree<T, Threads>::attemptBalanceLeft_nl(Node* nParent, Node* n, Node* nR, int hL0){
    scoped_lock lock(nR->lock);

    int hR = nR->height;
    if(hL0 - hR >= -1){
        return false;
    } else {
        Node* nRL = nR->left;
        int hRL0 = height(nRL);

        if(height(nR->right) >= hRL0){
            rotateLeft(nParent, n, hL0, nR, nRL, hRL0);
        } else {
            scoped_lock lock(nRL->lock);

            int hRL = nRL->height;
            if(height(nR->right) >= hRL){
                rotateLeft(nParent, n, hL0, nR, nRL, hRL);
            } else {
                rotateLeftOverRight(nParent, n, hL0, nR, nRL);
            }
        }

        return true;
    }
}
        
template<typename T, int Threads>
void AVLTree<T, Threads>::rotateRight(Node* nParent, Node* n, Node* nL, int hR, Node* nLR, int hLR){
    long nodeOVL = n->version;
    n->version = beginChange(nodeOVL);

    n->left = nLR;
    if(nLR){
        nLR->parent = n;
    }

    nL->right = n;
    n->parent = nL;

    if(nParent->left == n){
        nParent->left = nL;
    } else {
        nParent->right = nL;
    }
    nL->parent = nParent;

    int hNRepl = 1 + std::max(hLR, hR);
    n->height = hNRepl;
    nL->height = 1 + std::max(height(nL->left), hNRepl);

    n->version = endChange(nodeOVL);
}

template<typename T, int Threads>
void AVLTree<T, Threads>::rotateLeft(Node* nParent, Node* n, int hL, Node* nR, Node* nRL, int hRL){
    long nodeOVL = n->version;
    n->version = beginChange(nodeOVL);

    n->right = nRL;
    if(nRL){
        nRL->parent = n;
    }

    nR->left = n;
    n->parent = nR;

    if(nParent->left == n){
        nParent->left = nR;
    } else {
        nParent->right = nR;
    }

    nR->parent = nParent;

    int hNRepl = 1 + std::max(hL, hRL);
    n->height = hNRepl;
    nR->height = 1 + std::max(hNRepl, height(nR->right));

    n->version = endChange(nodeOVL);
}

template<typename T, int Threads>
void AVLTree<T, Threads>::rotateRightOverLeft(Node* nParent, Node* n, Node* nL, int hR, Node* nLR){
    long nodeOVL = n->version;
    long leftOVL = nL->version;
    n->version = beginChange(nodeOVL);
    nL->version = beginChange(leftOVL);

    Node* nLRL = nLR->left;
    Node* nLRR = nLR->right;

    n->left = nLRR;
    if(nLRR){
        nLRR->parent = n;
    }

    nL->right = nLRL;
    if(nLRL){
        nLRL->parent = nL;
    }

    nLR->left = nL;
    nL->parent = nLR;
    nLR->right = n;
    n->parent = nLR;

    if(nParent->left == n){
        nParent->left = nLR;
    } else {
        nParent->right = nLR;
    }
    nLR->parent = nParent;

    int hNRepl = 1 + std::max(height(nLRR), hR);
    n->height = hNRepl;

    int hLRepl = 1 + std::max(height(nL->left), height(nLRL));
    nL->height = hLRepl;

    nLR->height = 1 + std::max(hLRepl, hNRepl);

    n->version = endChange(nodeOVL);
    nL->version = endChange(leftOVL);
}

template<typename T, int Threads>
void AVLTree<T, Threads>::rotateLeftOverRight(Node* nParent, Node* n, int hL, Node* nR, Node* nRL){
    long nodeOVL = n->version;
    long rightOVL = nR->version;

    n->version = beginChange(nodeOVL);
    nR->version = beginChange(rightOVL);

    Node* nRLL = nRL->left;
    Node* nRLR = nRL->right;

    n->right = nRLL;
    if(nRLL){
        nRLL->parent = n;
    }

    nR->left = nRLR;
    if(nRLR){
        nRLR->parent = nR;
    }

    nRL->right = nR;
    nR->parent = nRL;
    nRL->left = n;
    n->parent = nRL;

    if(nParent->left == n){
        nParent->left = nRL;
    } else {
        nParent->right = nRL;
    }
    nRL->parent = nParent;

    int hNRepl = 1 + std::max(hL, height(nRLL));
    n->height = hNRepl;
    int hRRepl = 1 + std::max(height(nRLR), height(nR->right));
    nR->height = hRRepl;
    nRL->height = 1 + std::max(hNRepl, hRRepl);

    n->version = endChange(nodeOVL);
    nR->version = endChange(rightOVL);
}

template<typename T, int Threads>
bool AVLTree<T, Threads>::attemptUnlink_nl(Node* parent, Node* node){
    assert(!isUnlinked(parent->version));

    Node* parentL = parent->left;
    Node* parentR = parent->right;

    if(parentL != node && parentR != node){
        return false;
    }

    assert(!isUnlinked(node->version));
    assert(parent == node->parent);

    Node* left = node->left;
    Node* right = node->right;

    if(left && right){
        return false;
    }

    Node* splice = left ? left : right;
    
    if(parentL == node){
        parent->left = splice;
    } else {
        parent->right = splice;
    }

    if(splice){
        splice->parent = parent;
    }

    node->version = UnlinkedOVL;
    node->value = false;

    return true;
}

} //end of avltree

#endif
