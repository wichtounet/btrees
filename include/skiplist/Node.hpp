#include "hash.hpp"

namespace skiplist {

template<typename T>
class Node {
    public:
        const T value;
        const int key;
        Node<T>* next[MAX_LEVEL + 1];
        //Node<T>** next;//TODO Check if possible to declare directly static array
        const int length;
        int topLevel;

    public:
        Node() = delete;
        Node(const Node& rhs)  = delete;

        //constructor for sentinel nodes
        Node(int key);

        //constructor for ordinary nodes
        Node(T x, int height);

        ~Node();
};

template<typename T>
Node<T>::Node(int k) : value(k), key(k), length(MAX_LEVEL + 1), topLevel(MAX_LEVEL) {

    for(int i = 0; i < MAX_LEVEL + 1; ++i){
        next[i] = nullptr;
    }

    //next = (Node<T>**) calloc(length, sizeof(Node<T>*));
}

template<typename T>
Node<T>::Node(T x, int height) : value(x), key(hash(x)), length(height + 1), topLevel(height) {
    for(int i = 0; i < MAX_LEVEL + 1; ++i){
        next[i] = nullptr;
    }
    //next = (Node<T>**) calloc(length, sizeof(Node<T>*));
}

template<typename T>
Node<T>::~Node(){
    std::cout << "caca" << std::endl;
    //free(next);
}

}
