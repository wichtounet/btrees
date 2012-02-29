#include "hash.hpp"

namespace skiplist {

struct Node {
    //constructor for sentinel nodes
    Node(int key);

    //constructor for ordinary nodes
    Node(int key, int height);

    const int key;
    int topLevel;
    Node* next[MAX_LEVEL + 1];
};

Node::Node(int key) : key(key), topLevel(MAX_LEVEL) {}
Node::Node(int key, int height) : key(key), topLevel(height) {}

}
