#include <climits>

#include "Utils.hpp"

#include "skiplist/Node.hpp"

namespace skiplist {

template<typename T>
class SkipList {
    public:
        SkipList();
        ~SkipList();

        bool add(T value);
        bool remove(T value);
        bool contains(T value);

    private:
        bool find(T x, Node<T>** preds, Node<T>** succs);

        Node<T>* head;
        Node<T>* tail;
};

template<typename T>
SkipList<T>::SkipList(){
    head = new Node<T>(INT_MIN);
    tail = new Node<T>(INT_MAX);

    for(int i = 0; i < head->length; ++i){
        head->next[i] = tail;
    }
}

template<typename T>
SkipList<T>::~SkipList(){
    delete head;
    delete tail;
}

template<typename T>
bool SkipList<T>::add(T value){
    int topLevel = random(P, MAX_LEVEL);
    int bottomLevel = 0;

    Node<T>* preds[MAX_LEVEL + 1];
    Node<T>* succs[MAX_LEVEL + 1];

    while(true){
        if(find(value, preds, succs)){
            return false;
        } else {
            Node<T>* newNode = new Node<T>(value, topLevel);

            for(int level = bottomLevel; level <= topLevel; ++level){
                Set(&newNode->next[level], succs[level], false);
            }

            Node<T>* pred = preds[bottomLevel];
            Node<T>* succ = succs[bottomLevel]; 

            Set(&newNode->next[bottomLevel], succ, false);

            if(!CompareAndSet(&pred->next[bottomLevel], succ, newNode, false, false)){
                continue;
            }

            for(int level = bottomLevel + 1; level <= topLevel; ++level){
                while(true){
                    pred = preds[level];
                    succ = succs[level];

                    if(CompareAndSet(&pred->next[level], succ, newNode, false, false)){ 
                        break;
                    }

                    find(value, preds, succs);
                }
            }

            return true;
        }
    }
}

template<typename T>
bool SkipList<T>::remove(T value){
    int bottomLevel = 0;
    
    Node<T>* preds[MAX_LEVEL + 1];
    Node<T>* succs[MAX_LEVEL + 1];

    while(true){
        bool found = find(value, preds, succs);

        if(!found){
            return false;
        } else {
            Node<T>* nodeToRemove = succs[bottomLevel];

            for(int level = nodeToRemove->topLevel; level >= bottomLevel + 1; --level){
                Node<T>* succ = nodeToRemove->next[level];

                while(!isMarked(succ)){
                    CompareAndSet(&nodeToRemove->next[level], succ, succ, false, true);
                    succ = nodeToRemove->next[level];
                }
            }

            Node<T>* succ = nodeToRemove->next[bottomLevel];
            while(true){
                bool iMarkedIt = CompareAndSet(&nodeToRemove->next[bottomLevel], succ, succ, false, true);

                succ = succs[bottomLevel]->next[bottomLevel];

                if(iMarkedIt){
                    find(value, preds, succs);
                    return true;
                } else if(isMarked(succ)){//Someone else marked it
                    return false;//TODO Check if that's correct in the context of multiset
                }
            }
        }
    }
}

template<typename T>
bool SkipList<T>::contains(T value){
    int bottomLevel = 0;

    int v = hash(value);

    Node<T>* pred = head;
    Node<T>* curr = nullptr;
    Node<T>* succ = nullptr;

    for(int level = MAX_LEVEL; level >= bottomLevel; --level){
        curr = pred->next[level];

        while(true){
            succ = curr->next[level];

            while(isMarked(succ)){
               curr = pred->next[level];
               succ = curr->next[level]; 
            }

            if(curr->key < v){
                pred = curr;
                curr = succ;
            } else {
                break;
            }
        }
    }

    return curr->key == v;
}

template<typename T>
bool SkipList<T>::find(T value, Node<T>** preds, Node<T>** succs){
    int bottomLevel = 0;
    int key = hash(value);

    Node<T>* pred = nullptr;
    Node<T>* curr = nullptr;
    Node<T>* succ = nullptr;

    while(true){
        retry:

        pred = head;

        for(int level = MAX_LEVEL; level >= bottomLevel; --level){
            curr = pred->next[level];

            while(true){
                succ = curr->next[level];

                while(isMarked(succ)){
                    if(!CompareAndSet(&pred->next[level], curr, succ, false, false)){
                        goto retry;
                    }

                    curr = pred->next[level];
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

}
