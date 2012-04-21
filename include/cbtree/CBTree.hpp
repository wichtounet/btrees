#ifndef CB_TREE
#define CB_TREE

//#define DEBUG_LINK

//Helper to unlink node
#ifdef DEBUG_LINK
#define UNLINK(parent, node) if((!node->left || !node->right) && !node->value){attemptUnlink_nl(parent, node);}
#define CHECK(node) if(node && !isUnlinked(node->changeOVL) && (!node->left || !node->right) && !node->value){assert(!"Node needs unlink");}
#else
#define UNLINK(parent, node)
#define CHECK(node)
#endif

#include <mutex>
#include <atomic>

#include "Utils.hpp"
#include "HazardManager.hpp"

namespace cbtree {

typedef std::lock_guard<std::mutex> scoped_lock;
    
static const int SpinCount = 100;

static const char Left = 'L';
static const char Right = 'R';

/* Version stuff */
static const int OVLBitsBeforeOverflow = 8;
static const long UnlinkedOVL = 1L;
static const long OVLGrowLockMask = 2L;
static const long OVLShrinkLockMask = 4L;
static const int OVLGrowCountShift = 3;
static const long OVLGrowCountMask = ((1L << OVLBitsBeforeOverflow) - 1) << OVLGrowCountShift;
static const long OVLShrinkCountShift = OVLGrowCountShift + OVLBitsBeforeOverflow;

#define NEW_LOG_CALCULATION_THRESHOLD 15 //log(2*Threads²)

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

    Node* child(char dir){
        return dir == Left ? left : right;
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

        std::atomic<int> size;
        std::atomic<int> logSize;
        int local_size[Threads];
        
        void publish(Node* ref);
        void releaseAll();

        HazardManager<Node, Threads, 6> hazard;
        
        unsigned int Current[Threads];
        void deep_release(Node* node);
        
        /* Allocate new nodes */
        Node* newNode(int key, bool value, Node* parent, long changeOVL, Node* left, Node* right);

        /* Internal stuff  */
        Result getImpl(int key);
        Result update(int key);
        Result attemptRemove(int key, Node* parent, Node* node, long nodeOVL, int height); 
        Result attemptGet(int key, Node* node, char dirToc, long nodeOVL, int height);
        bool attemptInsertIntoEmpty(int key);
        Result attemptUpdate(int key, Node* parent, Node* node, long nodeOVL, int height);
        Result attemptNodeUpdate(bool newValue, Node* parent, Node* node);
        bool attemptUnlink_nl(Node* parent, Node* node);

        /* Balancing */
        void SemiSplay(Node* child);
        void RebalanceAtTarget(Node* parent, Node* node);
        void RebalanceNew(Node* parent, char dirToC);

        /* Rotating stuff */
        void rotateRight(Node* nParent, Node* n, Node* nL, Node* nLR);
        void rotateLeft(Node* nParent, Node* n, Node* nR, Node* nRL);
        void rotateRightOverLeft(Node* nParent, Node* n, Node* nL, Node* nLR);
        void rotateLeftOverRight(Node* nParent, Node* n, Node* nR, Node* nRL);
};

template<typename T, int Threads>
CBTree<T, Threads>::CBTree(){
    rootHolder = newNode(std::numeric_limits<int>::min(), false, nullptr, 0L, nullptr, nullptr); 
    rootHolder->ncnt = std::numeric_limits<int>::max();

    size.store(0);
    logSize.store(-1);

    for(int i = 0; i < Threads; ++i){
        local_size[i] = 0;
        Current[i] = 0;
    }
}

template<typename T, int Threads>
void CBTree<T, Threads>::deep_release(Node* node){
    if(node->left){
        deep_release(node->left);
    }
    
    if(node->right){
        deep_release(node->right);
    }

    if(!isUnlinked(node->changeOVL)){
        hazard.releaseNode(node);
    }
}

template<typename T, int Threads>
CBTree<T, Threads>::~CBTree(){
    deep_release(rootHolder);
}

template<typename T, int Threads>
void CBTree<T, Threads>::publish(Node* ref){
    hazard.publish(ref, Current[thread_num]);
    ++Current[thread_num];
}

template<typename T, int Threads>
void CBTree<T, Threads>::releaseAll(){
    for(unsigned int i = 0; i < Current[thread_num]; ++i){
        hazard.release(i);
    }
    
    Current[thread_num] = 0;
}

template<typename T, int Threads>
Node* CBTree<T, Threads>::newNode(int key, bool value, Node* parent, long changeOVL, Node* left, Node* right){
    Node* node = hazard.getFreeNode();
    
    node->key = key;
    node->value = value;
    node->parent = parent;
    node->changeOVL = changeOVL;
    node->left = left;
    node->right = right;

    node->ncnt = 1;
    node->rcnt = 0;
    node->lcnt = 0;

    return node;
}

template<typename T, int Threads>
bool CBTree<T, Threads>::contains(T value){
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

            long ovl = right->changeOVL;
            if(isShrinkingOrUnlinked(ovl)){
                right->waitUntilChangeCompleted(ovl);
            } else if(right == rootHolder->right){
                Result vo = attemptGet(key, right, (rightCmp < 0 ? Left : Right), ovl, 1);
                if(vo != RETRY){
                    return vo == FOUND;
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
                int log_size = logSize.load();

                if(height >= ((log_size << 2))){
                    SemiSplay(child);
                } else {
                    RebalanceAtTarget(node, child);
                }

                CHECK(child);
                CHECK(node);

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

template<typename T, int Threads>
bool CBTree<T, Threads>::add(T value){
    if(update(hash(value)) == NOT_FOUND){
        int log_size = logSize.load();

        if(log_size < NEW_LOG_CALCULATION_THRESHOLD){
            int new_size = ++size;
            int next_log_size = log_size + 1;
            if(new_size >= (1 << next_log_size)){
                logSize.compare_exchange_weak(log_size, next_log_size);
            }
        } else {
            ++local_size[thread_num];

            if(local_size[thread_num] >= Threads){
                int new_size = (size += local_size[thread_num]);
                local_size[thread_num] = 0;
                int next_log_size = log_size + 1;
                if(new_size >= (1 << next_log_size)){
                    logSize.compare_exchange_weak(log_size, next_log_size);
                }
            }
        }

        return true;
    } else {
        return false;
    }
}

template<typename T, int Threads>
bool CBTree<T, Threads>::remove(T value){
    int key = hash(value);

    while(true){
        Node* right = rootHolder->right;

        if(!right){
            releaseAll();

            return false;
        } else {
            long ovl = right->changeOVL;
            if(isShrinkingOrUnlinked(ovl)){
                right->waitUntilChangeCompleted(ovl);
            } else if (right == rootHolder->right){
                Result vo = attemptRemove(key, rootHolder, right, ovl, 1);
                
                CHECK(right);

                if(vo != RETRY){
                    if(vo == FOUND){
                        int log_size = logSize.load();
                        if(log_size < NEW_LOG_CALCULATION_THRESHOLD){
                            int new_size = --size;
                            if(new_size < (1 << log_size)){
                                logSize.compare_exchange_weak(log_size, log_size - 1);
                            }
                        } else {
                            --local_size[thread_num];
                            if(local_size[thread_num] <= -Threads){
                                int new_size = (size += local_size[thread_num]);
                                local_size[thread_num] = 0;
                                if(new_size < (1 << log_size)){
                                    logSize.compare_exchange_weak(log_size, log_size - 1);
                                }
                            }
                        }

                        releaseAll();

                        return true;
                    } else {
                        releaseAll();

                        return false;
                    }
                }
            }
        }
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

                CHECK(right);
                
                if(vo != RETRY){
                    return vo;
                }
            }
        }
    }
}

template<typename T, int Threads>
bool CBTree<T, Threads>::attemptInsertIntoEmpty(int key){
    publish(rootHolder);
    scoped_lock lock(rootHolder->lock);

    if(!rootHolder->right){
        rootHolder->right = newNode(key, true, rootHolder, 0L, nullptr, nullptr);
        releaseAll();
        return true;
    } else {
        releaseAll();
        return false;
    }
}

template<typename T, int Threads>
Result CBTree<T, Threads>::attemptUpdate(int key, Node* parent, Node* node, long nodeOVL, int height){
    assert(nodeOVL != UnlinkedOVL);

    int cmp = key - node->key;
    if(cmp == 0){
        int log_size = logSize.load();

        if(height >= ((log_size << 2))){
            SemiSplay(node);
        } else {
            RebalanceAtTarget(parent, node);
        }

        CHECK(parent);
        CHECK(node);

        ++node->ncnt;
        return attemptNodeUpdate(true, parent, node);
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
                publish(node);
                scoped_lock lock(node->lock);
        
                if(hasShrunkOrUnlinked(nodeOVL, node->changeOVL)){
                    releaseAll();
                    return RETRY;
                }

                if(node->child(dirToC)){
                    //retry
                } else {
                    node->setChild(dirToC, newNode(key, true, node, 0L, nullptr, nullptr));

                    if(dirToC == Left){
                        ++node->lcnt;
                    } else {
                        ++node->rcnt;
                    }

                    int log_size = logSize.load();
                    if(height >= ((log_size << 2 ))){
                       doSemiSplay = true; 
                    } else {
                        releaseAll();
                        return NOT_FOUND;
                    }
                }
        
                CHECK(node);
                    
                releaseAll();
            }

            if(doSemiSplay){
                SemiSplay(node->child(dirToC));
                CHECK(node);
                CHECK(node->child(dirToC));
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
                    
                CHECK(node);
                CHECK(child);

                if(vo != RETRY){
                    if(vo == NOT_FOUND){
                        RebalanceNew(node, dirToC);
                    } else {
                        if(dirToC == Left){
                            ++node->lcnt;
                        } else {
                            ++node->rcnt;
                        }
                    }
                
                    CHECK(node);

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
        publish(parent);
        scoped_lock parentLock(parent->lock);

        if(isUnlinked(parent->changeOVL) || node->parent != parent){
            releaseAll();
            return RETRY;
        }

        publish(node);
        scoped_lock lock(node->lock);
        if(!node->value){
            releaseAll();
            return NOT_FOUND;
        }

        if(!attemptUnlink_nl(parent, node)){
            releaseAll();
            return RETRY;
        }

        //At this point, the parent can need unlink
        
        releaseAll();

        return FOUND;
    } else {
        publish(node);
        scoped_lock lock(node->lock);

        if(isUnlinked(node->changeOVL)){
            releaseAll();
            return RETRY;
        }

        if(!newValue && (!node->left || !node->right)){
            releaseAll();
            return RETRY;
        }

        bool prev = node->value;;
        node->value = newValue;

        if (!newValue && (!node->left || !node->right)) {
            releaseAll();
            return RETRY;
        }
        
        releaseAll();
        
        CHECK(node);

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

    hazard.releaseNode(node);

    return true;
}

template<typename T, int Threads>
Result CBTree<T, Threads>::attemptRemove(int key, Node* parent, Node* node, long nodeOVL, int height){
    assert(nodeOVL != UnlinkedOVL);

    int cmp = key - node->key;
    if(cmp == 0){
        return attemptNodeUpdate(false, parent, node);
    }

    char dirToC = cmp < 0 ? Left : Right;

    while(true){
        Node* child = node->child(dirToC);

        if(hasShrunkOrUnlinked(nodeOVL, node->changeOVL)){
            return RETRY;
        }

        if(!child){
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
                CHECK(node);
                CHECK(child);
                if(vo != RETRY){
                    return vo;
                }
            }
        }
    }
}
        
template<typename T, int Threads>
void CBTree<T, Threads>::SemiSplay(Node* child){
    while(child && child->parent && child->parent->parent){
        Node* node = child->parent;
        Node* parent = node->parent;

        if(!parent){
            break;
        }
    
        if(!parent->parent){
            publish(parent);
            scoped_lock parentLock(parent->lock);

            if(parent->right == node){
                publish(node);
                scoped_lock nodeLock(node->lock);

                if(!isUnlinked(node->changeOVL)){
                    if(node->left == child){
                        publish(child);
                        scoped_lock lock(child->lock);
                        rotateRight(parent, node, child, child->right);
                    } else if(node->right == child){
                        publish(child);
                        scoped_lock lock(child->lock);
                        rotateLeft(parent, node, child, child->left);
                    }
                }
            }
        } else {
            Node* grand = parent->parent;
            publish(grand);
            scoped_lock lock(grand->lock);

            if(grand->left == parent || grand->right == parent){
                publish(parent);
                scoped_lock parentLock(parent->lock);

                if(parent->left == node){
                    publish(node);
                    scoped_lock nodeLock(node->lock);
                    
                    if(!isUnlinked(node->changeOVL)){
                        if(node->left == child){
                            publish(child);
                            scoped_lock childLock(child->lock);
                            rotateRight(grand, parent, node, node->right);
                            child = node;   
                        } else if(node->right == child){
                            publish(child);
                            scoped_lock childLock(child->lock);
                            rotateRightOverLeft(grand, parent, node, child);
                        }
                    }
                } else if (parent->right == node){
                    publish(node);
                    scoped_lock nodeLock(node->lock);
                    
                    if(!isUnlinked(node->changeOVL)){
                        if(node->right == child){
                            publish(child);
                            scoped_lock childLock(child->lock);
                            rotateLeft(grand, parent, node, node->left);
                            child = node;   
                        } else if(node->left == child){
                            publish(child);
                            scoped_lock childLock(child->lock);
                            rotateLeftOverRight(grand, parent, node, child);
                        }
                    }
                }
            }
        }

        releaseAll();
    }
}

template<typename T, int Threads>
void CBTree<T, Threads>::RebalanceAtTarget(Node* parent, Node* node){
    int ncnt;
    int pcnt;
    int n_other_cnt;

    if(parent->left == node){
        ncnt = node->ncnt + node->lcnt;
        pcnt = parent->ncnt + parent->rcnt;
        n_other_cnt = node->rcnt;
    } else {
        ncnt = node->ncnt + node->rcnt;
        pcnt = parent->ncnt + parent->lcnt;
        n_other_cnt = node->lcnt;
    }

    if(n_other_cnt >= pcnt){
        Node* grand = parent->parent;
        publish(grand);
        scoped_lock grandLock(grand->lock);

        if(grand->left == parent || grand->right == parent){
            publish(parent);
            scoped_lock parentLock(parent->lock);

            if(parent->left == node){
                publish(node);
                scoped_lock nodeLock(node->lock);

                Node* nR = node->right;

                if(nR){
                    publish(nR);
                    scoped_lock nRLock(nR->lock);

                    rotateRightOverLeft(grand, parent, node, nR);
                    parent->lcnt = nR->rcnt;
                    node->rcnt = nR->lcnt;
                    nR->rcnt += pcnt;
                    nR->lcnt += ncnt;
                }
            } else if(parent->right == node){
                publish(node);
                scoped_lock nodeLock(node->lock);

                Node* nL = node->left;

                if(nL){
                    publish(nL);
                    scoped_lock nLLock(nL->lock);

                    rotateLeftOverRight(grand, parent, node, nL);
                    parent->rcnt = nL->lcnt;
                    node->lcnt = nL->rcnt;
                    nL->lcnt += pcnt;
                    nL->rcnt += ncnt;
                }
            }
        }
    } else if(ncnt > pcnt){
        Node* grand = parent->parent;
        publish(grand);
        scoped_lock grandLock(grand->lock);

        if(grand->left == parent || grand->right == parent){
            publish(parent);
            scoped_lock parentLock(parent->lock);

            if(parent->left == node){
                publish(node);
                scoped_lock nodeLock(node->lock);
                rotateRight(grand, parent, node, node->right);
                parent->lcnt = node->rcnt;
                node->rcnt += pcnt;
            } else if(parent->right == node){
                publish(node);
                scoped_lock nodeLock(node->lock);
                rotateLeft(grand, parent, node, node->left);
                parent->rcnt = node->lcnt;
                node->lcnt += pcnt;
            }
        }
    }

    releaseAll();
}

template<typename T, int Threads>
void CBTree<T, Threads>::RebalanceNew(Node* parent, char dirToC){
    Node* node = parent->child(dirToC);
    int ncnt;
    int pcnt;
    int n_other_cnt;

    if(!node){
        if(dirToC == Left){
            ++parent->lcnt;
        } else {
            ++parent->rcnt;
        }
        return;
    }

    if(dirToC == Left){
        ncnt = node->ncnt + node->lcnt;
        pcnt = parent->ncnt + parent->rcnt;
        n_other_cnt = node->rcnt;
    } else {
        ncnt = node->ncnt + node->rcnt;
        pcnt = parent->ncnt + parent->lcnt;
        n_other_cnt = node->lcnt;
    }

    if(n_other_cnt >= pcnt){
        Node* grand = parent->parent;
        publish(grand);
        scoped_lock grandLock(grand->lock);

        if(grand->left == parent || grand->right == parent){
            publish(parent);
            scoped_lock parentLock(parent->lock);

            if(parent->left == node){
                publish(node);
                scoped_lock nodeLock(node->lock);

                Node* nR = node->right;
                if(nR){
                    publish(nR);
                    scoped_lock nRLock(nR->lock);
                    rotateRightOverLeft(grand, parent, node, nR);
                    parent->lcnt = nR->rcnt;
                    node->rcnt = nR->lcnt;
                    nR->rcnt += pcnt;
                    nR->lcnt += ncnt;
                }
            } else if(parent->right == node){
                publish(node);
                scoped_lock nodeLock(node->lock);

                Node* nL = node->left;
                if(nL){
                    publish(nL);
                    scoped_lock nLLock(nL->lock);
                    rotateLeftOverRight(grand, parent, node, nL);
                    parent->rcnt = nL->lcnt;
                    node->lcnt = nL->rcnt;
                    nL->lcnt += pcnt;
                    nL->rcnt += ncnt;
                }
            }
        }
    } else if(ncnt > pcnt){
        Node* grand = parent->parent;
        publish(grand);
        scoped_lock grandLock(grand->lock);
        if(grand->left == parent || grand->right == parent){
            publish(parent);
            scoped_lock parentLock(parent->lock);
            if(parent->left == node){
                publish(node);
                scoped_lock nodeLock(node->lock);
                rotateRight(grand, parent, node, node->right);
                parent->lcnt = node->rcnt;
                node->rcnt += pcnt;
            } else if(parent->right == node){
                publish(node);
                scoped_lock nodeLock(node->lock);
                rotateLeft(grand, parent, node, node->left);
                parent->rcnt = node->lcnt;
                node->lcnt += pcnt;
            }
        }
    } else {
        if(dirToC == Left){
            ++parent->lcnt;
        } else {
            ++parent->rcnt;
        }
    }

    releaseAll();
}

template<typename T, int Threads>
void CBTree<T, Threads>::rotateRight(Node* nParent, Node* n, Node* nL, Node* nLR){
    long nodeOVL = n->changeOVL;
    long leftOVL = nL->changeOVL;

    Node* nPL = nParent->left;

    n->changeOVL = beginShrink(nodeOVL);
    nL->changeOVL = beginGrow(leftOVL);

    n->left = nLR;
    nL->right = n;
    if(nPL == n){
        nParent->left = nL;
    } else {
        nParent->right = nL;
    }

    nL->parent = nParent;
    n->parent = nL;
    if(nLR){
        nLR->parent = n;
    }

    nL->changeOVL = endGrow(leftOVL);
    n->changeOVL = endShrink(nodeOVL);
}

template<typename T, int Threads>
void CBTree<T, Threads>::rotateLeft(Node* nParent, Node* n, Node* nR, Node* nRL){
    long nodeOVL = n->changeOVL;
    long rightOVL = nR->changeOVL;

    Node* nPL = nParent->left;

    n->changeOVL = beginShrink(nodeOVL);
    nR->changeOVL = beginGrow(rightOVL);

    n->right = nRL;
    nR->left = n;
    
    if(nPL == n){
        nParent->left = nR;
    } else {
        nParent->right = nR;
    }

    nR->parent = nParent;
    n->parent = nR;
    if(nRL){
        nRL->parent = n;
    }

    nR->changeOVL = endGrow(rightOVL);
    n->changeOVL = endShrink(nodeOVL);
}

template<typename T, int Threads>
void CBTree<T, Threads>::rotateRightOverLeft(Node* nParent, Node* n, Node* nL, Node* nLR){
    long nodeOVL = n->changeOVL;
    long leftOVL = nL->changeOVL;
    long leftROVL = nLR->changeOVL;

    Node* nPL = nParent->left;
    Node* nLRL = nLR->left;
    Node* nLRR = nLR->right;

    n->changeOVL = beginShrink(nodeOVL);
    nL->changeOVL = beginShrink(leftOVL);
    nLR->changeOVL = beginGrow(leftROVL);

    n->left = nLRR;
    nL->right = nLRL;
    nLR->left = nL;
    nLR->right = n;

    if(nPL == n){
        nParent->left = nLR;
    } else {
        nParent->right = nLR;
    }

    nLR->parent = nParent;
    nL->parent = nLR;
    n->parent = nLR;

    if(nLRR){
        nLRR->parent = n;
    }

    if(nLRL){
        nLRL->parent = nL;
    }

    nLR->changeOVL = endGrow(leftROVL);
    nL->changeOVL = endShrink(leftOVL);
    n->changeOVL = endShrink(nodeOVL);
}

template<typename T, int Threads>
void CBTree<T, Threads>::rotateLeftOverRight(Node* nParent, Node* n, Node* nR, Node* nRL){
    long nodeOVL = n->changeOVL;
    long rightOVL = nR->changeOVL;
    long rightLOVL = nRL->changeOVL;

    Node* nPL = nParent->left;
    Node* nRLL = nRL->left;
    Node* nRLR = nRL->right;

    n->changeOVL = beginShrink(nodeOVL);
    nR->changeOVL = beginShrink(rightOVL);
    nRL->changeOVL = beginGrow(rightLOVL);

    n->right = nRLL;
    nR->left = nRLR;
    nRL->right = nR;
    nRL->left = n;

    if(nPL == n){
        nParent->left = nRL;
    } else {
        nParent->right = nRL;    
    }

    nRL->parent = nParent;
    nR->parent = nRL;
    n->parent = nRL;

    if(nRLL){
        nRLL->parent = n;
    }

    if(nRLR){
        nRLR->parent = nR;
    }

    nRL->changeOVL = endGrow(rightLOVL);
    nR->changeOVL = endShrink(rightOVL);
    n->changeOVL = endShrink(nodeOVL);
}

} //end of cbtree

#endif
