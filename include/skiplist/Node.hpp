#include "hash.hpp"

namespace skiplist {

template<typename T>
class Node {
    public:
        const T value;
        const int key;
        Node<T>* next[MAX_LEVEL + 1];
        const int length;
        int topLevel;

    public:
        //constructor for sentinel nodes
        Node(int key);

        //constructor for ordinary nodes
        Node(T x, int height);
};

template<typename T>
Node<T>::Node(int k) : value(k), key(k), length(MAX_LEVEL + 1), topLevel(MAX_LEVEL) {
    for(int i = 0; i < MAX_LEVEL + 1; ++i){
        next[i] = nullptr;
    }
}

template<typename T>
Node<T>::Node(T x, int height) : value(x), key(hash(x)), length(height + 1), topLevel(height) {
    for(int i = 0; i < MAX_LEVEL + 1; ++i){
        next[i] = nullptr;
    }
}

}
