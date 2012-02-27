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
    int topLevel = randomLevel(MAX_LEVEL + 1);
    int bottomLevel = 0;

    Node<T>** preds = (Node<T>**) malloc(sizeof(Node<T>*) * (MAX_LEVEL + 1));
    Node<T>** succs = (Node<T>**) malloc(sizeof(Node<T>*) * (MAX_LEVEL + 1));

    while(true){
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
bool SkipList<T>::remove(T/* value*/){
    //TODO
    return false;
}

template<typename T>
bool SkipList<T>::contains(T/* value*/){
    //TODO
    return false;
}

template<typename T>
bool SkipList<T>::find(T /*x*/, Node<T>** /*preds*/, Node<T>** /*succs*/){
    //TODO
    return false;
}

}
