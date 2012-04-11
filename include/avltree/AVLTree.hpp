#ifndef AVL_TREE_TREE
#define AVL_TREE_TREE

#include <mutex>
#include <cassert>

#include "hash.hpp"

namespace avltree {

typedef std::lock_guard<std::mutex> scoped_lock;

static int SpinCount = 100;

static long beginChange(long ovl) { return ovl | 1; }
static long endChange(long ovl) { return (ovl | 3) + 1; }

static bool isShrinking(long ovl) { return (ovl & 1) != 0; }
static bool isUnlinked(long ovl) { return (ovl & 2) != 0; }
static bool isShrinkingOrUnlinked(long ovl) { return (ovl & 3) != 0L; }

static long UnlinkedOVL = 2;

/* Conditions on nodes */
static const int UnlinkRequired = -1;
static const int RebalanceRequired = -2;
static const int NothingRequired = -3;

enum Function {
    UpdateIfPresent, 
    UpdateIfAbsent
};

struct Node {
    int height;
    int key;
    long version;
    bool value;
    Node* parent;
    Node* left;
    Node* right;

    Node* nextNode; //For hazard pointers

    std::mutex lock;

    Node() : nextNode(nullptr) {}

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
        ~AVLTree();
        
        bool contains(T value);
        bool add(T value);
        bool remove(T value);

    private:
        Node* rootHolder;

        HazardManager<Node, Threads, 6> hazard;
    
        /* Allocate new nodes */
        Node* newNode(int key);
        Node* newNode(int height, int key, long version, bool value, Node* parent, Node* left, Node* right);

        //Search
        Result attemptGet(int key, Node* node, int dir, long nodeV);

        /* Update stuff  */
        Result updateUnderRoot(int key, Function func, bool expected, bool newValue, Node* holder);
        bool attemptInsertIntoEmpty(int key, bool value, Node* holder);
        Result attemptUpdate(int key, Function func, bool expected, bool newValue, Node* parent, Node* node, long nodeOVL);
        Result attemptNodeUpdate(Function func, bool expected, bool newValue, Node* parent, Node* node);
        bool attemptUnlink_nl(Node* parent, Node* node);
        
        /* To wait during shrinking  */
        void waitUntilNotChanging(Node* node);
        
        /* Rebalancing stuff  */
        void fixHeightAndRebalance(Node* node);
        Node* rebalance_nl(Node* nParent, Node* n);
        Node* rebalanceToRight_nl(Node* nParent, Node* n, Node* nL, int hR0);
        Node* rebalanceToLeft_nl(Node* nParent, Node* n, Node* nR, int hL0);

        /* Rotation stuff */        
        Node* rotateLeftOverRight_nl(Node* nParent, Node* n, int hL, Node* nR, Node* nRL, int hRR, int hRLR);
        Node* rotateRightOverLeft_nl(Node* nParent, Node* n, Node* nL, int hR, int hLL, Node* nLR, int hLRL);
        Node* rotateLeft_nl(Node* nParent, Node* n, int hL, Node* nR, Node* nRL, int hRL, int hRR);
        Node* rotateRight_nl(Node* nParent, Node* n, Node* nL, int hR, int hLL, Node* nLR, int hLR);
};

static Node* fixHeight_nl(Node* n);
static int height(Node* node);
static int nodeCondition(Node* node);

template<typename T, int Threads>
AVLTree<T, Threads>::AVLTree(){
    rootHolder = newNode(std::numeric_limits<int>::min());
}

template<typename T, int Threads>
AVLTree<T, Threads>::~AVLTree(){
    hazard.releaseNode(rootHolder);
}
    
template<typename T, int Threads>
Node* AVLTree<T, Threads>::newNode(int key){
    return newNode(1, key, 0L, false, nullptr, nullptr, nullptr);
}

template<typename T, int Threads>
Node* AVLTree<T, Threads>::newNode(int height, int key, long version, bool value, Node* parent, Node* left, Node* right){
    Node* node = hazard.getFreeNode();
    
    node->height = height;
    node->key = key;
    node->version = version;
    node->value = value;
    node->parent = parent;
    node->left = left;
    node->right = right;
    
    node->nextNode = nullptr;

    return node;
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

inline bool shouldUpdate(Function func, bool prev, bool/* expected*/){
    return func == UpdateIfAbsent ? !prev : prev;
}

inline Result updateResult(Function func, bool/* prev*/){
    return func == UpdateIfAbsent ? NOT_FOUND : FOUND;
}

inline Result noUpdateResult(Function func, bool/* prev*/){
    return func == UpdateIfAbsent ? FOUND : NOT_FOUND;
}

template<typename T, int Threads>
bool AVLTree<T, Threads>::add(T value){
    return updateUnderRoot(hash(value), UpdateIfAbsent, false, true, rootHolder) == NOT_FOUND;
}

template<typename T, int Threads>
bool AVLTree<T, Threads>::remove(T value){
    return updateUnderRoot(hash(value), UpdateIfPresent, true, false, rootHolder) == FOUND;
}

template<typename T, int Threads>
Result AVLTree<T, Threads>::updateUnderRoot(int key, Function func, bool expected, bool newValue, Node* holder){
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
    hazard.publish(holder, 0);
    scoped_lock lock(holder->lock);

    if(!holder->right){
        holder->right = newNode(1, key, 0, value, holder, nullptr, nullptr);
        holder->height = 2;
        hazard.releaseAll();
        return true;
    } else {
        hazard.releaseAll();
        return false;
    }
}

template<typename T, int Threads>
Result AVLTree<T, Threads>::attemptUpdate(int key, Function func, bool expected, bool newValue, Node* parent, Node* node, long nodeOVL){
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
                    hazard.publish(node, 0);
                    scoped_lock lock(node->lock);    
                
                    if(node->version != nodeOVL){
                        hazard.releaseAll();
                        return RETRY;
                    }

                    if(node->child(cmp)){
                        success = false;
                        damaged = nullptr;
                    } else {
                        if(!shouldUpdate(func, false, expected)){
                            hazard.releaseAll();
                            return noUpdateResult(func, false);
                        }

                        Node* newChild = newNode(1, key, 0, true, node, nullptr, nullptr);
                        node->setChild(cmp, newChild);

                        success = true;
                        damaged = fixHeight_nl(node);
                    }
                    
                    hazard.releaseAll();
                }

                if(success){
                    fixHeightAndRebalance(damaged);
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
Result AVLTree<T, Threads>::attemptNodeUpdate(Function func, bool expected, bool newValue, Node* parent, Node* node){
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
            hazard.publish(parent, 0);
            scoped_lock parentLock(parent->lock);
            
            if(isUnlinked(parent->version) || node->parent != parent){
                hazard.releaseAll();
                return RETRY;
            }

            {
                hazard.publish(node, 1);
                scoped_lock lock(node->lock);
                
                prev = node->value;

                if(!shouldUpdate(func, prev, expected)){
                    hazard.releaseAll();
                    return noUpdateResult(func, prev);
                }

                if(!prev){
                    hazard.releaseAll();
                    return updateResult(func, prev);
                }

                if(!attemptUnlink_nl(parent, node)){
                    hazard.releaseAll();
                    return RETRY;
                }
            }
            
            hazard.releaseAll();

            damaged = fixHeight_nl(parent);
        }

        fixHeightAndRebalance(damaged);

        return updateResult(func, prev);
    } else {
        hazard.publish(node, 0);
        scoped_lock lock(node->lock);

        if(isUnlinked(node->version)){
            hazard.releaseAll();
            return RETRY;
        }

        bool prev = node->value;
        if(!shouldUpdate(func, prev, expected)){
            hazard.releaseAll();
            return noUpdateResult(func, prev);
        }

        if(!newValue && (!node->left || !node->right)){
            hazard.releaseAll();
            return RETRY;
        }

        node->value = newValue;
        
        hazard.releaseAll();
        return updateResult(func, prev);
    }
}

template<typename T, int Threads>
void AVLTree<T, Threads>::waitUntilNotChanging(Node* node){
    //hazard.publish(node, 0);
    
    long version = node->version;

    if(isShrinking(version)){
        for(int i = 0; i < SpinCount; ++i){
            if(version != node->version){
                //hazard.releaseAll();
                return;
            }
        }
            
        node->lock.lock();
        node->lock.unlock();
    }

    //hazard.releaseAll();
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
    while(node && node->parent){
        int condition = nodeCondition(node);
        if(condition == NothingRequired || isUnlinked(node->version)){
            return;
        }

        if(condition != UnlinkRequired && condition != RebalanceRequired){
            hazard.publish(node, 0);
            scoped_lock lock(node->lock);
            
            node = fixHeight_nl(node);

            hazard.releaseAll();
        } else {
            Node* nParent = node->parent;
            hazard.publish(nParent, 0);
            scoped_lock lock(nParent->lock);

            if(!isUnlinked(nParent->version) && node->parent == nParent){
                hazard.publish(node, 1);
                scoped_lock nodeLock(node->lock);

                node = rebalance_nl(nParent, node);
            }

            hazard.releaseAll();
        }
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
Node* AVLTree<T, Threads>::rebalance_nl(Node* nParent, Node* n){
    Node* nL = n->left;
    Node* nR = n->right;

    if((!nL || !nR) && !n->value){
        if(attemptUnlink_nl(nParent, n)){
            return fixHeight_nl(nParent);
        } else {
            return n;
        }
    }
    
    int hN = n->height;
    int hL0 = height(nL);
    int hR0 = height(nR);
    int hNRepl = 1 + std::max(hL0, hR0);
    int bal = hL0 - hR0;

    if(bal > 1){
        return rebalanceToRight_nl(nParent, n, nL, hR0);
    } else if(bal < -1){
        return rebalanceToLeft_nl(nParent, n, nR, hL0);
    } else if(hNRepl != hN) {
        n->height = hNRepl;

        return fixHeight_nl(nParent);
    } else {
        return nullptr;
    }
}

template<typename T, int Threads>
Node* AVLTree<T, Threads>::rebalanceToRight_nl(Node* nParent, Node* n, Node* nL, int hR0){
    hazard.publish(nL, 2);
    scoped_lock lock(nL->lock);

    int hL = nL->height;
    if(hL - hR0 <= 1){
        return n;
    } else {
        Node* nLR = nL->right;
        int hLL0 = height(nL->left);
        int hLR0 = height(nLR);

        if(hLL0 > hLR0){
            return rotateRight_nl(nParent, n, nL, hR0, hLL0, nLR, hLR0);
        } else {
            {
                hazard.publish(nLR, 3);
                scoped_lock subLock(nLR->lock);

                int hLR = nLR->height;
                if(hLL0 >= hLR){
                    return rotateRight_nl(nParent, n, nL, hR0, hLL0, nLR, hLR);
                } else {
                    int hLRL = height(nLR->left);
                    int b = hLL0 - hLRL;
                    if(b >= -1 && b <= 1){
                        return rotateRightOverLeft_nl(nParent, n, nL, hR0, hLL0, nLR, hLRL);
                    }
                }
            }

            return rebalanceToLeft_nl(n, nL, nLR, hLL0);
        }
    }
}

template<typename T, int Threads>
Node* AVLTree<T, Threads>::rebalanceToLeft_nl(Node* nParent, Node* n, Node* nR, int hL0){
    hazard.publish(nR, 4);
    scoped_lock lock(nR->lock);

    int hR = nR->height;
    if(hL0 - hR >= -1){
        return n;
    } else {
        Node* nRL = nR->left;
        int hRL0 = height(nRL);
        int hRR0 = height(nR->right);

        if(hRR0 >= hRL0){
            return rotateLeft_nl(nParent, n, hL0, nR, nRL, hRL0, hRR0);
        } else {
            {
                hazard.publish(nRL, 5);
                scoped_lock subLock(nRL->lock);

                int hRL = nRL->height;
                if(hRR0 >= hRL){
                    return rotateLeft_nl(nParent, n, hL0, nR, nRL, hRL, hRR0);
                } else {
                    int hRLR = height(nRL->right);
                    int b = hRR0 - hRLR;
                    if(b >= -1 && b <= 1){
                        return rotateLeftOverRight_nl(nParent, n, hL0, nR, nRL, hRR0, hRLR);
                    }
                }
            }

            return rebalanceToRight_nl(n, nR, nRL, hRR0);
        }
    }
}
        
template<typename T, int Threads>
Node* AVLTree<T, Threads>::rotateRight_nl(Node* nParent, Node* n, Node* nL, int hR, int hLL, Node* nLR, int hLR){
    long nodeOVL = n->version;
    Node* nPL = nParent->left;
    n->version = beginChange(nodeOVL);

    n->left = nLR;
    if(nLR){
        nLR->parent = n;
    }

    nL->right = n;
    n->parent = nL;

    if(nPL == n){
        nParent->left = nL;
    } else {
        nParent->right = nL;
    }
    nL->parent = nParent;

    int hNRepl = 1 + std::max(hLR, hR);
    n->height = hNRepl;
    nL->height = 1 + std::max(hLL, hNRepl);

    n->version = endChange(nodeOVL);

    int balN = hLR - hR;
    if(balN < -1 || balN > 1){
        return n;
    }

    int balL = hLL - hNRepl;
    if(balL < -1 || balL > 1){
        return nL;
    }

    return fixHeight_nl(nParent);
}

template<typename T, int Threads>
Node* AVLTree<T, Threads>::rotateLeft_nl(Node* nParent, Node* n, int hL, Node* nR, Node* nRL, int hRL, int hRR){
    long nodeOVL = n->version;
    Node* nPL = nParent->left;
    n->version = beginChange(nodeOVL);

    n->right = nRL;
    if(nRL){
        nRL->parent = n;
    }

    nR->left = n;
    n->parent = nR;

    if(nPL == n){
        nParent->left = nR;
    } else {
        nParent->right = nR;
    }
    nR->parent = nParent;

    int hNRepl = 1 + std::max(hL, hRL);
    n->height = hNRepl;
    nR->height = 1 + std::max(hNRepl, hRR);

    n->version = endChange(nodeOVL);

    int balN = hRL - hL;
    if(balN < -1 || balN > 1){
        return n;
    }

    int balR = hRR - hNRepl;
    if(balR < -1 || balR > 1){
        return nR;
    }

    return fixHeight_nl(nParent);
}

template<typename T, int Threads>
Node* AVLTree<T, Threads>::rotateRightOverLeft_nl(Node* nParent, Node* n, Node* nL, int hR, int hLL, Node* nLR, int hLRL){
    long nodeOVL = n->version;
    long leftOVL = nL->version;

    Node* nPL = nParent->left;
    Node* nLRL = nLR->left;
    Node* nLRR = nLR->right;
    int hLRR = height(nLRR);

    n->version = beginChange(nodeOVL);
    nL->version = beginChange(leftOVL);

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

    if(nPL == n){
        nParent->left = nLR;
    } else {
        nParent->right = nLR;
    }
    nLR->parent = nParent;

    int hNRepl = 1 + std::max(hLRR, hR);
    n->height = hNRepl;

    int hLRepl = 1 + std::max(hLL, hLRL);
    nL->height = hLRepl;

    nLR->height = 1 + std::max(hLRepl, hNRepl);

    n->version = endChange(nodeOVL);
    nL->version = endChange(leftOVL);

    assert(std::abs(hLL - hLRL) <= 1);

    int balN = hLRR - hR;
    if(balN < -1 || balN > 1){
        return n;
    }

    int balLR = hLRepl - hNRepl;
    if(balLR < -1 || balLR > 1){
        return nLR;
    }
    
    return fixHeight_nl(nParent);
}

template<typename T, int Threads>
Node* AVLTree<T, Threads>::rotateLeftOverRight_nl(Node* nParent, Node* n, int hL, Node* nR, Node* nRL, int hRR, int hRLR){
    long nodeOVL = n->version;
    long rightOVL = nR->version;

    n->version = beginChange(nodeOVL);
    nR->version = beginChange(rightOVL);

    Node* nPL = nParent->left;
    Node* nRLL = nRL->left;
    Node* nRLR = nRL->right;
    int hRLL = height(nRLL);

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

    if(nPL == n){
        nParent->left = nRL;
    } else {
        nParent->right = nRL;
    }
    nRL->parent = nParent;

    int hNRepl = 1 + std::max(hL, hRLL);
    n->height = hNRepl;
    int hRRepl = 1 + std::max(hRLR, hRR);
    nR->height = hRRepl;
    nRL->height = 1 + std::max(hNRepl, hRRepl);

    n->version = endChange(nodeOVL);
    nR->version = endChange(rightOVL);
    
    assert(std::abs(hRR - hRLR) <= 1);

    int balN = hRLL - hL;
    if(balN < -1 || balN > 1){
        return n;
    }

    int balRL = hRRepl - hNRepl;
    if(balRL < -1 || balRL > 1){
        return nRL;
    }
    
    return fixHeight_nl(nParent);
}

} //end of avltree

#endif
