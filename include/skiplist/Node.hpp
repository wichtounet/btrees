#include "skiplist/Constants.hpp"

#include "hash.hpp"

namespace skiplist {
    
template<typename T>
class Node {
    public:
        const T value;
        const int key;
        Node<T>* next;//TODO Check if possible to declare directly static array

    private:
        int toplevel;

    public:
        //constructor for sentinel nodes
        Node(int key);

        //constructor for ordinary nodes
        Node(T x, int height);
};

template<typename T>
Node<T>::Node(int k) : value(k), key(k) {
    //TODO
}

template<typename T>
Node<T>::Node(T x, int height) : value(x), key(hash(x)) {
    //TODO
}

}
