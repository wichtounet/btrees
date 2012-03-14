#include "hash.hpp"

namespace skiplist {

struct Node {
    Node();

    int key;
    int topLevel;
    Node* next[MAX_LEVEL + 1];

    Node* nextNode; //For the hazard manager
};

Node::Node() : nextNode(nullptr) {
    //Nothing to do here
}

}
