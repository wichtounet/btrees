#include <climits>

#include "Utils.hpp"

#include "skiplist/Node.hpp"

namespace skiplist {

template<typename T>
class SkipList {
    public:
        SkipList();

        void add(T value);
        bool remove(T value);
        bool contains(T value);

    private:
        bool find(T x, Node<T>** preds, Node<T>** succs);

        Node<T> head;
        Node<T> tail;
};

template<typename T>
SkipList<T>::SkipList() : head(INT_MIN), tail(INT_MAX) {
    for(int i = 0; i < head.length; ++i){
        head.next[i] = &tail;   
    }
}

template<typename T>
void SkipList<T>::add(T value){
    int topLevel = random(P, MAX_LEVEL);
    int bottomLevel = 0;

    Node<T>** preds = (Node<T>**) malloc(sizeof(Node<T>*) * (MAX_LEVEL + 1));
    Node<T>** succs = (Node<T>**) malloc(sizeof(Node<T>*) * (MAX_LEVEL + 1));

    while(true){
        find(value, preds, succs);

        Node<T>* newNode = new Node<T>(value, topLevel);

        for(int level = bottomLevel; level <= topLevel; ++level){
            Node<T>* succ = succs[level];

            newNode->next[level] = succ; //(succ,false)
        }

        Node<T>* pred = preds[bottomLevel];
        Node<T>* succ = succs[bottomLevel]; 

        newNode->next[bottomLevel] = succ; //(succ,false)

        if(!CASPTR(&pred->next[bottomLevel], succ, newNode)){ //(succ, false) -> (newNode, false)
            continue;
        }

        for(int level = bottomLevel + 1; level <= topLevel; ++level){
            while(true){
                pred = preds[level];
                succ = succs[level];

                if(CASPTR(&pred->next[level], succ, newNode)){ //(succ, false) -> (newNode, false)
                    break;
                }

                find(value, preds, succs);
            }
        }

        return;
    }
}

template<typename T>
bool SkipList<T>::remove(T value){
    int bottomLevel = 0;
    
    Node<T>** preds = (Node<T>**) malloc(sizeof(Node<T>*) * (MAX_LEVEL + 1));
    Node<T>** succs = (Node<T>**) malloc(sizeof(Node<T>*) * (MAX_LEVEL + 1));

    while(true){
        bool found = find(value, preds, succs);

        if(!found){
            return false;
        } else {
            Node<T>* nodeToRemove = succs[bottomLevel];

            for(int level = nodeToRemove->topLevel; level >= bottomLevel + 1; --level){
                while(!isMarked(nodeToRemove->next[level])){
                    CONVERSION<Node<T>> current;
                    current.node = nodeToRemove->next[level];
                    current.value &= (~0 - 1);

                    CONVERSION<Node<T>> next;
                    next.node = nodeToRemove->next[level];
                    next.value |= 0x1;
                    
                    CASPTR(&nodeToRemove->next[level], current.node, next.node);
                }
            }

            while(true){
                CONVERSION<Node<T>> current;
                current.node = nodeToRemove->next[bottomLevel];
                current.value &= (~0 - 1);
                
                CONVERSION<Node<T>> next;
                next.node = nodeToRemove->next[bottomLevel];
                next.value |= 0x1;

                if(CASPTR(&nodeToRemove->next[bottomLevel], current.node, next.node)){
                    find(value, preds, succs);
                    return true;
                } else if(isMarked(nodeToRemove->next[bottomLevel])){
                    return false;
                }
            }
        }
    }
}

template<typename T>
bool SkipList<T>::contains(T value){
    int bottomLevel = 0;

    int v = hash(value);

    Node<T>* pred = &head;
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

        pred = &head;

        for(int level = MAX_LEVEL; level >= bottomLevel; --level){
            curr = pred->next[level];

            while(true){
                succ = curr->next[level];

                while(isMarked(curr->next[level])){
                    CONVERSION<Node<T>> current;
                    current.node = curr;
                    current.value &= (~0 - 1);

                    CONVERSION<Node<T>> next;
                    next.node = succ;
                    next.value &= (~0 - 1);

                    if(!CASPTR(&pred->next[level], current.node, next.node)){
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
