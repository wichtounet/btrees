#ifndef CB_TREE
#define CB_TREE

#include <climits>
#include <mutex>

#include "Utils.hpp"
#include "HazardManager.hpp"

namespace cbtree {
    
static const int SpinCount = 100;
static const int OVLBitsBeforeOverflow = 8;

static const char Left = 'L';
static const char Right = 'R';
static const char Middle = 'M';

/* Version stuff */
static long UnlinkedOVL = 1L;
static long OVLGrowLockMask = 2L;
static long OVLShrinkLockMask = 4L;
static int OVLGrowCountShift = 3;
static long OVLGrowCountMask = ((1L << OVLBitsBeforeOverflow) - 1) << OVLGrowCountShift;
static long OVLShrinkCountShift = OVLGrowCountShift + OVLBitsBeforeOverflow;

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
                    return vo == NOT_FOUND ? true : false;
                }
            }
        }
    }
}

} //end of cbtree

#endif
