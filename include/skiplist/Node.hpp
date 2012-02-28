#include "hash.hpp"

namespace skiplist {

template<typename T>
struct Node {
    //constructor for sentinel nodes
    Node(int key);

    //constructor for ordinary nodes
    Node(T x, int height);

    const int key;
    int topLevel;
    Node<T>* next[MAX_LEVEL + 1];
};

template<typename T>
Node<T>::Node(int key) : key(key), topLevel(MAX_LEVEL) {}

template<typename T>
Node<T>::Node(T value, int height) : key(hash(value)), topLevel(height) {}

}
