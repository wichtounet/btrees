#ifndef SKIP_LIST
#define SKIP_LIST

#include <climits>

#include "Utils.hpp"
#include "HazardManager.hpp"

#define MAX_LEVEL 25 //Should be choosen as log(1/p)(n)
#define P 0.5        //probability for randomLevel (geometric distribution)

#include "skiplist/Node.hpp"

namespace skiplist {

inline Node* Unmark(Node* node){
    return reinterpret_cast<Node*>(reinterpret_cast<unsigned long>(node) & (~0l - 1));
}

inline Node* Mark(Node* node){
    return reinterpret_cast<Node*>(reinterpret_cast<unsigned long>(node) | 0x1);
}

inline bool IsMarked(Node* node){
    return reinterpret_cast<unsigned long>(node) & 0x1;
}

template<typename T, int Threads>
class SkipList {
    public:
        SkipList();
        ~SkipList();

        bool add(T value);
        bool remove(T value);
        bool contains(T value);

    private:
        int randomLevel();
        bool find(int key, Node** preds, Node** succs);

        Node* newNode(int key);
        Node* newNode(int key, int height);

        Node* head;
        Node* tail;

        HazardManager<Node, Threads, 3> hazard;

        std::mt19937_64 engine;
        std::geometric_distribution<int> distribution;
};

template<typename T, int Threads>
Node* SkipList<T, Threads>::newNode(int key){
    Node* node = hazard.getFreeNode();

    node->key = key;

    return node;
}

template<typename T, int Threads>
Node* SkipList<T, Threads>::newNode(int key, int height){
    Node* node = hazard.getFreeNode();

    node->key = key;
    node->topLevel = height;

    return node;
}

template<typename T, int Threads>
SkipList<T, Threads>::SkipList() : engine(time(NULL)), distribution(P) {
    head = newNode(INT_MIN);
    tail = newNode(INT_MAX);

    head->topLevel = MAX_LEVEL;

    for(int i = 0; i < MAX_LEVEL + 1; ++i){
        head->next[i] = tail;
    }

    tail->topLevel = 0;
}

template<typename T, int Threads>
SkipList<T, Threads>::~SkipList(){
    delete head;
    delete tail;
}

template<typename T, int Threads>
int SkipList<T, Threads>::randomLevel(){
    int level = distribution(engine); 
    
    return (level >= MAX_LEVEL) ? MAX_LEVEL : level;
}

template<typename T, int Threads>
bool SkipList<T, Threads>::add(T value){
    int key = hash(value);
    int topLevel = randomLevel();

    Node* preds[MAX_LEVEL + 1];
    Node* succs[MAX_LEVEL + 1];
            
    Node* newElement = newNode(key, topLevel);
    hazard.publish(newElement, 0);

    if(!newElement){
        std::cout << "shit" << std::endl;
    }

    while(true){
        if(find(key, preds, succs)){
            hazard.releaseAll();
            
            hazard.releaseNode(newElement);

            return false;
        } else {
            for(int level = 0; level <= topLevel; ++level){
                newElement->next[level] = succs[level];
            }

            hazard.publish(preds[0]->next[0], 1);
            hazard.publish(succs[0], 2);
            
            if(CASPTR(&preds[0]->next[0], succs[0], newElement)){
                for(int level = 1; level <= topLevel; ++level){
                    while(true){
                        hazard.publish(preds[level]->next[level], 1);
                        hazard.publish(succs[level], 2);

                        if(CASPTR(&preds[level]->next[level], succs[level], newElement)){ 
                            break;
                        } else {
                            find(key, preds, succs);
                        }
                    }
                }
            
                hazard.releaseAll();

                return true;
            }
        }
    }
}

template<typename T, int Threads>
bool SkipList<T, Threads>::remove(T value){
    int key = hash(value);

    Node* preds[MAX_LEVEL + 1];
    Node* succs[MAX_LEVEL + 1];

    while(true){
        if(!find(key, preds, succs)){
            hazard.release(1);
            hazard.release(0);

            return false;
        } else {
            Node* nodeToRemove = succs[0];
            hazard.publish(nodeToRemove, 0);

            for(int level = nodeToRemove->topLevel; level > 0; --level){
                Node* succ = nullptr;
                do {
                    succ = nodeToRemove->next[level];
                    hazard.publish(succ, 1);

                    if(IsMarked(succ)){
                        break;
                    }
                } while (!CASPTR(&nodeToRemove->next[level], succ, Mark(succ)));
            }

            while(true){
                Node* succ = nodeToRemove->next[0];
                hazard.publish(succ, 1);

                if(IsMarked(succ)){
                    break;
                } else if(CASPTR(&nodeToRemove->next[0], succ, Mark(succ))){
                    hazard.release(1);
                    hazard.release(0);
                    
                    find(key, preds, succs);
                    
                    hazard.releaseNode(nodeToRemove);

                    return true;
                }
            }
        }
    }
}

template<typename T, int Threads>
bool SkipList<T, Threads>::contains(T value){
    int key = hash(value);

    Node* pred = head;
    Node* curr = nullptr;
    Node* succ = nullptr;

    for(int level = MAX_LEVEL; level >= 0; --level){
        curr = Unmark(pred->next[level]);

        while(true){
            succ = curr->next[level];

            while(IsMarked(succ)){
                curr = Unmark(curr->next[level]);
                succ = curr->next[level]; 
            }

            if(curr->key < key){
                pred = curr;
                curr = succ;
            } else {
                break;
            }
        }
    }

    bool found = curr->key == key;

    return found;
}

template<typename T, int Threads>
bool SkipList<T, Threads>::find(int key, Node** preds, Node** succs){
    Node* pred = nullptr;
    Node* curr = nullptr;
    Node* succ = nullptr;
        
retry:
    //We must do to release after the goto
    hazard.releaseAll();
    
    pred = head;
    hazard.publish(pred, 0);

    for(int level = MAX_LEVEL; level >= 0; --level){
        curr = pred->next[level];
        hazard.publish(curr, 1);

        while(true){
            if(IsMarked(curr)){
                goto retry;
            }

            succ = curr->next[level];
            hazard.publish(succ, 2);

            while(IsMarked(succ)){
                if(!CASPTR(&pred->next[level], curr, Unmark(succ))){
                    goto retry;
                }

                curr = pred->next[level];
                hazard.publish(curr, 1);

                if(IsMarked(curr)){
                    goto retry;
                }

                succ = curr->next[level];
                hazard.publish(succ, 2);
            }

            if(curr->key < key){
                pred = curr;
                hazard.publish(pred, 0);
                
                curr = succ;
                hazard.publish(curr, 1);
            } else {
                break;
            }
        }

        preds[level] = pred;
        succs[level] = curr;
    }

    bool found = curr->key == key;
    
    hazard.releaseAll();

    return found;
}

}

#endif
