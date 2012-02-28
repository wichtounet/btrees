#include <climits>

#include "Utils.hpp"

#include "skiplist/Node.hpp"

namespace skiplist {

#define Mark(a) (Node<T>*)((unsigned long)(a) | 0x1)
#define Unmark(a) (Node<T>*)((unsigned long)(a) & (~0l - 1))
#define IsMarked(a) ((unsigned long)(a) & 0x1)

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

    Node<T>* preds[MAX_LEVEL + 1];
    Node<T>* succs[MAX_LEVEL + 1];
            
    Node<T>* newNode = new Node<T>(value, topLevel);

    while(true){
        if(find(value, preds, succs)){
            delete newNode;

            return false;
        } else {
            for(int level = 0; level <= topLevel; ++level){
                newNode->next[level] = succs[level];
            }

            if(CASPTR(&preds[0]->next[0], succs[0], newNode)){
                for(int level = 1; level <= topLevel; ++level){
                    while(true){
                        if(CASPTR(&preds[level]->next[level], succs[level], newNode)){ 
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
    Node<T>* preds[MAX_LEVEL + 1];
    Node<T>* succs[MAX_LEVEL + 1];

    while(true){
        if(!find(value, preds, succs)){
            return false;
        } else {
            Node<T>* nodeToRemove = succs[0];

            for(int level = nodeToRemove->topLevel; level > 0; --level){
                Node<T>* succ = nullptr;
                do {
                    succ = nodeToRemove->next[level];

                    if(IsMarked(succ)){
                        break;
                    }
                } while (!CASPTR(&nodeToRemove->next[level], succ, Mark(succ)));
            }

            while(true){
                Node<T>* succ = nodeToRemove->next[0];

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

    Node<T>* pred = head;
    Node<T>* curr = nullptr;
    Node<T>* succ = nullptr;

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
bool SkipList<T>::find(T value, Node<T>** preds, Node<T>** succs){
    int key = hash(value);

    Node<T>* pred = nullptr;
    Node<T>* curr = nullptr;
    Node<T>* succ = nullptr;
        
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
