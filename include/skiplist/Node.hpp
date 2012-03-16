#include "hash.hpp"

#include <cstring>

namespace skiplist {

struct Node {
    Node();
    ~Node();

    int key;
    int topLevel;
    Node** next;

    Node* nextNode; //For the hazard manager
};

Node::Node() : nextNode(nullptr) {
    //Fill the array with null pointers
    next = (Node**) calloc(MAX_LEVEL + 1, sizeof(Node *));
}

Node::~Node(){
    free(next);
}

}
