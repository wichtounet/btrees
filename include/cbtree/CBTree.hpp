#ifndef CB_TREE
#define CB_TREE

#include <climits>
#include <mutex>

#include "Utils.hpp"
#include "HazardManager.hpp"

namespace cbtree {

typedef std::lock_guard<std::mutex> scoped_lock;
    
static const int SpinCount = 100;
static const int OVLBitsBeforeOverflow = 8;

static const char Left = 'L';
static const char Right = 'R';
static const char Middle = 'M';

/* Functions */ 
static const int UpdateAlways = 0;
static const int UpdateIfAbsent = 1;
static const int UpdateIfPresent = 2;
static const int UpdateIfEq = 3;

/* Version stuff */
static const long UnlinkedOVL = 1L;
static const long OVLGrowLockMask = 2L;
static const long OVLShrinkLockMask = 4L;
static const int OVLGrowCountShift = 3;
static const long OVLGrowCountMask = ((1L << OVLBitsBeforeOverflow) - 1) << OVLGrowCountShift;
static const long OVLShrinkCountShift = OVLGrowCountShift + OVLBitsBeforeOverflow;

static bool isChanging( long ovl) {
    return (ovl & (OVLShrinkLockMask | OVLGrowLockMask)) != 0;
}

static bool isUnlinked( long ovl) {
    return ovl == UnlinkedOVL;
}

static bool isShrinkingOrUnlinked( long ovl) {
    return (ovl & (OVLShrinkLockMask | UnlinkedOVL)) != 0;
}

static bool isChangingOrUnlinked( long ovl) {
    return (ovl & (OVLShrinkLockMask | OVLGrowLockMask | UnlinkedOVL)) != 0;
}

static bool hasShrunkOrUnlinked( long orig,  long current) {
    return ((orig ^ current) & ~(OVLGrowLockMask | OVLGrowCountMask)) != 0;
}

static bool hasChangedOrUnlinked( long orig,  long current) {
    return orig != current;
}

static long beginGrow( long ovl) {
    assert(!isChangingOrUnlinked(ovl));
    return ovl | OVLGrowLockMask;
}

static long endGrow( long ovl) {
    assert(!isChangingOrUnlinked(ovl));

    // Overflows will just go into the shrink lock count, which is fine.
    return ovl + (1L << OVLGrowCountShift);
}

static long beginShrink( long ovl) {
    assert(!isChangingOrUnlinked(ovl));

    return ovl | OVLShrinkLockMask;
}

static long endShrink( long ovl) {
    assert(!isChangingOrUnlinked(ovl));

    // increment overflows directly
    return ovl + (1L << OVLShrinkCountShift);
}

struct Node {
    int key;
    bool value;

    Node* parent;
    long changeOVL;

    Node* left;
    Node* right;
    
    int ncnt;
    int rcnt;
    int lcnt;

    std::mutex lock;

    Node(int key, bool value, Node* parent, long changeOVL, Node* left, Node* right) : key(key), value(value), parent(parent), changeOVL(changeOVL), left(left), right(right), ncnt(1), rcnt(0), lcnt(0) {
        //Nothing    
    }

    Node* child(char dir){
        return dir == Left ? left : right;
    }

    Node* childSibling(char dir){
        return dir == Left ? right : left;
    }
    
    void setChild(char dir, Node* node) {
        if (dir == Left) {
            left = node;
        } else {
            right = node;
        }
    }

    void waitUntilChangeCompleted(long ovl) {
        if (!isChanging(ovl)) {
            return;
        }

        for (int tries = 0; tries < SpinCount; ++tries) {
            if (changeOVL != ovl) {
                return;
            }
        }

        lock.lock();
        lock.unlock();

        assert(changeOVL != ovl);
    }
};

enum Result {
    FOUND,
    NOT_FOUND,
    RETRY
};

template<typename T, int Threads>
class CBTree {
    public:
        CBTree();
        ~CBTree();

        bool add(T value);
        bool remove(T value);
        bool contains(T value);

    private:
        Node* rootHolder;

        Result getImpl(int key);
        Result update(int key);
        Result attemptRemove(int key, Node* parent, Node* node, long nodeOVL, int height); 
        Result attemptGet(int key, Node* node, char dirToc, long nodeOVL, int height);
        bool attemptInsertIntoEmpty(int key);
        Result attemptUpdate(int key, Node* parent, Node* node, long nodeOVL, int height);
        Result attemptNodeUpdate(bool newValue, Node* parent, Node* node);
        bool attemptUnlink_nl(Node* parent, Node* node);
};

template<typename T, int Threads>
CBTree<T, Threads>::CBTree(){
    rootHolder = new Node(INT_MIN, false, nullptr, 0L, nullptr, nullptr); 

    //TODO init local sizes if necessary
}

template<typename T, int Threads>
CBTree<T, Threads>::~CBTree(){
    //TODO
}

template<typename T, int Threads>
bool CBTree<T, Threads>::contains(T value){
    return getImpl(hash(value)) == FOUND;
}

template<typename T, int Threads>
bool CBTree<T, Threads>::add(T value){
    return update(hash(value)) == NOT_FOUND;    
}

template<typename T, int Threads>
bool CBTree<T, Threads>::remove(T value){
    int key = hash(value);

    while(true){
        Node* right = rootHolder->right;

        if(!right){
            return NOT_FOUND;
        } else {
            long ovl = right->changeOVL;
            if(isShrinkingOrUnlinked(ovl)){
                right->waitUntilChangeCompleted(ovl);
            } else if (right == rootHolder->right){
                Result vo = attemptRemove(key, rootHolder, right, ovl, 1);

                if(vo != RETRY){
                    return vo == FOUND ? true : false;
                }
            }
        }
    }
}

template<typename T, int Threads>
Result CBTree<T, Threads>::getImpl(int key){
    while(true){
        Node* right = rootHolder->right;
        if(!right){
            return NOT_FOUND;
        } else {
            int rightCmp = key - right->key;
            if(rightCmp == 0){
                return right->value ? FOUND : NOT_FOUND;
            }

            long ovl = right->changeOVL;
            if(isShrinkingOrUnlinked(ovl)){
                right->waitUntilChangeCompleted(ovl);
            } else if(right == rootHolder->right){
                Result vo = attemptGet(key, right, (rightCmp < 0 ? Left : Right), ovl, 1);
                if(vo != RETRY){
                    return vo;
                }
            }
        }
    }
}

template<typename T, int Threads>
Result CBTree<T, Threads>::attemptGet(int key, Node* node, char dirToC, long nodeOVL, int height){
    while(true){
        Node* child = node->child(dirToC);

        if(!child){
            if(hasShrunkOrUnlinked(nodeOVL, node->changeOVL)){
                return RETRY;
            }

            return NOT_FOUND;
        } else {
            int childCmp = key - child->key;
            if(childCmp == 0){
                //TODO Rebalance
                ++child->ncnt;
                return child->value ? FOUND : NOT_FOUND;
            }

            long childOVL = child->changeOVL;
            if(isShrinkingOrUnlinked(childOVL)){
                child->waitUntilChangeCompleted(childOVL);

                if(hasShrunkOrUnlinked(nodeOVL, node->changeOVL)){
                    return RETRY;
                }
            } else if(child != node->child(dirToC)){
                if(hasShrunkOrUnlinked(nodeOVL, node->changeOVL)){
                    return RETRY;
                }
            } else {
                if(hasShrunkOrUnlinked(nodeOVL, node->changeOVL)){
                    return RETRY;
                }

                Result vo = attemptGet(key, child, (childCmp < 0 ? Left : Right), childOVL, height + 1);
                if(vo != RETRY){
                    if(vo != NOT_FOUND){
                        if(dirToC == Left){
                            ++node->lcnt;
                        } else {
                            ++node->rcnt;
                        }
                    }

                    return vo;
                }
            }
        }
    }
}

static bool shouldUpdate(int func, bool prev, bool expected) {
    switch (func) {
        case UpdateAlways:
            return true;
        case UpdateIfAbsent:
            return !prev;
        case UpdateIfPresent:
            return !prev;
        default:
            return prev == expected; 
    }
}

template<typename T, int Threads>
Result CBTree<T, Threads>::update(int key){
    while(true){
        Node* right = rootHolder->right;
        if(!right){
            if(attemptInsertIntoEmpty(key)){
                return NOT_FOUND;
            }
        } else {
            long ovl = right->changeOVL;
            if(isShrinkingOrUnlinked(ovl)){
                right->waitUntilChangeCompleted(ovl);
            } else if(right == rootHolder->right){
                Result vo = attemptUpdate(key, rootHolder, right, ovl, 1);
                if(vo != RETRY){
                    return vo;
                }
            }
        }
    }
}

template<typename T, int Threads>
Result CBTree<T, Threads>::attemptUpdate(int key, Node* parent, Node* node, long nodeOVL, int height){
    assert(nodeOVL != UnlinkedOVL);

    int cmp = key - node->key;
    if(cmp == 0){
        //TODO Rebalance stuff
        ++node->ncnt;
        Result result = attemptNodeUpdate(true, parent, node);
        if(result != RETRY){
            //increment height
        }
        return result;
    }

    char dirToC = cmp < 0 ? Left : Right;

    while(true){
        Node* child = node->child(dirToC);

        if(hasShrunkOrUnlinked(nodeOVL, node->changeOVL)){
            return RETRY;
        }

        if(!child){
            bool doSemiSplay = false;

            {
                scoped_lock lock(node->lock);
        
                if(hasShrunkOrUnlinked(nodeOVL, node->changeOVL)){
                    return RETRY;
                }

                if(node->child(dirToC)){
                    //retry
                } else {
                    node->setChild(dirToC, new Node(key, true, node, 0L, nullptr, nullptr));

                    if(dirToC == Left){
                        ++node->lcnt;
                    } else {
                        ++node->rcnt;
                    }

                    //TODO Balancing stuff

                    return NOT_FOUND;
                }
            }

            if(doSemiSplay){
                //SemiSplay(node->child(dirToC));
                return NOT_FOUND;
            }
        } else {
            long childOVL = child->changeOVL;
            if(isShrinkingOrUnlinked(childOVL)){
                child->waitUntilChangeCompleted(childOVL);
            } else if(child != node->child(dirToC)){
                //Retry
            } else {
                if(hasShrunkOrUnlinked(nodeOVL, node->changeOVL)){
                    return RETRY;
                }

                Result vo = attemptUpdate(key, node, child, childOVL, height + 1);

                if(vo != RETRY){
                    //TODO Balancing stuff

                    return vo;
                }
            }
        }
    }
}

template<typename T, int Threads>
Result CBTree<T, Threads>::attemptNodeUpdate(bool newValue, Node* parent, Node* node){
    if(!newValue){
        if(!node->value){
            return NOT_FOUND;
        }
    }

    if(!newValue && (!node->left || !node->right)){
        bool prev;
        scoped_lock parentLock(parent->lock);

        if(isUnlinked(parent->changeOVL) || node->parent != parent){
            return RETRY;
        }

        scoped_lock lock(node->lock);
        prev = node->value;
        if(!prev){
            return NOT_FOUND;
        }

        if(!attemptUnlink_nl(parent, node)){
            return RETRY;
        }

        return FOUND;
    } else {
        scoped_lock lock(node->lock);

        if(isUnlinked(node->changeOVL)){
            return RETRY;
        }

        bool prev = node->value;

        if(!newValue && (!node->left || !node->right)){
            return RETRY;
        }

        node->value = newValue;
        return prev ? FOUND : NOT_FOUND;
    }
}

template<typename T, int Threads>
bool CBTree<T, Threads>::attemptUnlink_nl(Node* parent, Node* node){
    assert(!isUnlinked(parent->changeOVL));

    Node* parentL = parent->left;
    Node* parentR = parent->right;

    if(parentL != node && parentR != node){
        return false;
    }

    assert(!isUnlinked(node->changeOVL));
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

    node->changeOVL = UnlinkedOVL;
    node->value = false;

    return true;
}

template<typename T, int Threads>
Result CBTree<T, Threads>::attemptRemove(int key, Node* parent, Node* node, long nodeOVL, int height){
    assert(nodeOVL != UnlinkedOVL);

    int cmp = key - node->key;
    if(cmp == 0){
        Result res = attemptNodeUpdate(false, parent, node);
        if(res != RETRY){
            //INC height
        }
        return res;
    }

    char dirToC = cmp < 0 ? Left : Right;

    while(true){
        Node* child = node->child(dirToC);

        if(hasShrunkOrUnlinked(nodeOVL, node->changeOVL)){
            return RETRY;
        }

        if(!child){
            //Increment height
            return NOT_FOUND;
        } else {
            long childOVL = child->changeOVL;

            if(isShrinkingOrUnlinked(childOVL)){
                child->waitUntilChangeCompleted(childOVL);
            } else if(child != node->child(dirToC)){
                //Retry
            } else {
                if(hasShrunkOrUnlinked(nodeOVL, node->changeOVL)){
                    return RETRY;
                }

                Result vo = attemptRemove(key, node, child, childOVL, height + 1);
                if(vo != RETRY){
                    return vo;
                }
            }
        }
    }
}

template<typename T, int Threads>
bool CBTree<T, Threads>::attemptInsertIntoEmpty(int key){
   scoped_lock lock(rootHolder->lock);
   
   if(!rootHolder->right){
        rootHolder->right = new Node(key, true, rootHolder, 0L, nullptr, nullptr);
        return true;
   } else {
        return false;
   }
}

} //end of cbtree

#endif
