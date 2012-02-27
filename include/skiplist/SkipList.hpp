#include <climits>

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
        bool find(T x, Node<T>* preds, Node<T>* succs);

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
   //TODO 
}

template<typename T>
bool SkipList<T>::remove(T value){
    //TODO
    return false;
}

template<typename T>
bool SkipList<T>::contains(T value){
    //TODO
    return false;
}

}
