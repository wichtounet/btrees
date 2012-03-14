#include <climits>

#include "Utils.hpp"
#include "HazardManager.hpp"

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

template<typename T>
class SkipList {
    public:
        SkipList();
        ~SkipList();

        bool add(T value);
        bool remove(T value);
        bool contains(T value);

    private:
        bool find(T x, Node** preds, Node** succs);

        Node* newNode(int key);
        Node* newNode(int key, int height);

        Node* head;
        Node* tail;

        HazardManager<Node, 2> hazard;
};

template<typename T>
Node* SkipList<T>::newNode(int key){
    Node* node = hazard.getFreeNode();

    node->key = key;

    return node;
}

template<typename T>
Node* SkipList<T>::newNode(int key, int height){
    Node* node = hazard.getFreeNode();

    node->key = key;
    node->topLevel = height;

    return node;
}

template<typename T>
SkipList<T>::SkipList(){
    head = newNode(INT_MIN);
    tail = newNode(INT_MAX);

    for(int i = 0; i < MAX_LEVEL + 1; ++i){
        head->next[i] = tail;
    }

    tail->topLevel = 0;
}

template<typename T>
SkipList<T>::~SkipList(){
    delete head;
    delete tail;
}

template<typename T>
bool SkipList<T>::add(T value){
    int topLevel = random(P, MAX_LEVEL);

    Node* preds[MAX_LEVEL + 1];
    Node* succs[MAX_LEVEL + 1];
            
    Node* newElement = newNode(hash(value), topLevel);

    while(true){
        if(find(value, preds, succs)){
            delete newElement;

            return false;
        } else {
            for(int level = 0; level <= topLevel; ++level){
                newElement->next[level] = succs[level];
            }

            if(CASPTR(&preds[0]->next[0], succs[0], newElement)){
                for(int level = 1; level <= topLevel; ++level){
                    while(true){
                        if(CASPTR(&preds[level]->next[level], succs[level], newElement)){ 
                            break;
                        } else {
                            find(value, preds, succs);
                        }
                    }
                }

                return true;
            }
        }
    }
}

template<typename T>
bool SkipList<T>::remove(T value){
    Node* preds[MAX_LEVEL + 1];
    Node* succs[MAX_LEVEL + 1];

    while(true){
        if(!find(value, preds, succs)){
            return false;
        } else {
            Node* nodeToRemove = succs[0];

            for(int level = nodeToRemove->topLevel; level > 0; --level){
                Node* succ = nullptr;
                do {
                    succ = nodeToRemove->next[level];

                    if(IsMarked(succ)){
                        break;
                    }
                } while (!CASPTR(&nodeToRemove->next[level], succ, Mark(succ)));
            }

            while(true){
                Node* succ = nodeToRemove->next[0];

                if(IsMarked(succ)){
                    break;
                } else if(CASPTR(&nodeToRemove->next[0], succ, Mark(succ))){
                    find(value, preds, succs);
                    return true;
                }
            }
        }
    }
}

template<typename T>
bool SkipList<T>::contains(T value){
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

    return curr->key == key;
}

template<typename T>
bool SkipList<T>::find(T value, Node** preds, Node** succs){
    int key = hash(value);

    Node* pred = nullptr;
    Node* curr = nullptr;
    Node* succ = nullptr;
        
retry:
    pred = head;

    for(int level = MAX_LEVEL; level >= 0; --level){
        curr = pred->next[level];

        while(true){
            if(IsMarked(curr)){
                goto retry;
            }

            succ = curr->next[level];

            while(IsMarked(succ)){
                if(!CASPTR(&pred->next[level], curr, Unmark(succ))){
                    goto retry;
                }

                curr = pred->next[level];

                if(IsMarked(curr)){
                    goto retry;
                }

                succ = curr->next[level];
            }

            if(curr->key < key){
                pred = curr;
                curr = succ;
            } else {
                break;
            }
        }

        preds[level] = pred;
        succs[level] = curr;
    }

    return curr->key == key;
}

}
