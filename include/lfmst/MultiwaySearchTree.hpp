#ifndef MULTIWAY_SEARCH_TREE
#define MULTIWAY_SEARCH_TREE

#include <vector>

#include "hash.hpp"
#include "Utils.hpp"

//Lock-Free Multiway Search Tree
namespace lfmst {

struct Node;

//In this data structure keys can be null or POSITIVE_INFINITY
enum class KeyFlag : unsigned char {
    NORMAL,
    EMPTY,
    INF //POSITIVE_INFINITY
};

struct Key {
    KeyFlag flag;
    int key;

    Key() : flag(KeyFlag::EMPTY), key(0) {}
    Key(KeyFlag flag, int key) : flag(flag), key(key) {}

    bool operator==(const Key& rhs){
        return flag == rhs.flag && key == rhs.key;
    }
    
    bool operator!=(const Key& rhs){
        return !(*this == rhs);
    }
};

//Hold a set of key include the length of the set
struct Keys {
    Key* elements;
    const int length;

    Keys(int length) : length(length){
        elements = new Key[length];
    }

    Key& operator[](int index){
        return elements[index];
    }
};

//Hold a set of key including the lenght of the set
struct Children {
    Node** elements;
    const int length;

    Children(int length) : length(length){
        elements = (Node**) calloc(length, sizeof(Node*));
    }

    Node*& operator[](int index){
        return elements[index];
    }
};

struct Contents {
    Keys* items;
    Children* children;
    Node* link;

    Contents(Keys* items, Children* children, Node* link) : items(items), children(children), link(link)  {};
};

struct Node {
    Contents* contents;

    Node(Contents* contents) : contents(contents) {}

    bool casContents(Contents* cts, Contents* newCts){
        return CASPTR(&contents, cts, newCts);
    }
};

struct Search {
    Node* node;
    Contents* contents;
    int index;

    Search();
    Search(Node* node, Contents* contents, int index) : node(node), contents(contents), index(index) {}
};

struct HeadNode {
    Node* node;
    const int height;

    HeadNode(Node* node, int height) : node(node), height(height) {}
};

template<typename T, int Threads>
class MultiwaySearchTree {
    public:
        MultiwaySearchTree();
        
        bool contains(T value);
        bool add(T value);
        bool remove(T value);

    private:
        HeadNode* root;

        int search(Keys* items, Key key);
        int randomLevel();
        void traverseAndTrack(T value, int h, Search** srchs);
        Search* traverseAndCleanup(T value);
        bool insertList(T value, Search** srchs, Node* child, int h);
        Node* splitList(T value, Search** srch);
        Search* moveForward(Node* node, Key key);
        HeadNode* increaseRootHeight(int height);
        Node* cleanLink(Node* node, Contents* cts);
        void cleanNode(Key key, Node* node, Contents* cts, int index, Key leftBarrier);
};

/* Some internal utilities */ 
bool cleanNode1(Node* node, Contents* contents, Key leftBarrier);
bool cleanNode2(Node* node, Contents* contents, Key leftBarrier);
bool cleanNodeN(Node* node, Contents* contents, int index, Key leftBarrier);
Node* pushRight(Node* node, Key leftBarrier);
bool attemptSlideKey(Node* node, Contents* contents);
bool shiftChild(Node* node, Contents* contents, int index, Node* adjustedChild);
bool shiftChildren(Node* node, Contents* contents, Node* child1, Node* child2);
bool dropChild(Node* node, Contents* contents, int index, Node* adjustedChild);

template<typename T>
Key special_hash(T value){
    int key = hash(value);

    return {KeyFlag::NORMAL, key};
}

template<typename T, int Threads>
MultiwaySearchTree<T, Threads>::MultiwaySearchTree(){
    //init
}

template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::contains(T value){
    Key key = special_hash(value);

    Node* node = root->node;
    Contents* cts = node->contents;

    int i = search(cts->items, key);
    while(cts->children){
        if(-i -1 == cts->items->length){
            node = cts->link;
        } else if(i < 0){
            node = (*cts->children)[-i -1];
        } else {
            node = (*cts->children)[i];
        }

        cts = node->contents;
        i = search(cts->items, key);
    }

    while(true){
        if(-i -1 == cts->items->length){
            node = cts->link;
        } else if(i < 0){
            return false;
        } else {
            return true;
        }

        cts = node->contents;
        i = search(cts->items, key);
    }
}

template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::add(T value){
    int height = randomLevel();
    Search** srchs = (Search**) calloc(height + 1, sizeof(Search*));
    traverseAndTrack(value, height, srchs);
    bool success = insertList(value, srchs, nullptr, 0);
    if(!success){
        return false;
    }

    for(int i = 0; i < height; ++i){
        Node* right = splitList(value, &srchs[i]);
        insertList(value, srchs, right, i + 1);
    }

    return true;
}
        
template<typename T, int Threads>
void MultiwaySearchTree<T, Threads>::traverseAndTrack(T value, int h, Search** srchs){
    Key key = special_hash(value);

    HeadNode* root = this->root;

    if(root->height < h){
        root = increaseRootHeight(h);
    }

    int height = root->height;
    Node* node = root->node;
    Search* res = nullptr;
    while(true){
        Contents* cts = node->contents;
        int i = search(cts->items, key);

        if(-i -1 == cts->items->length){
            node = cts->link;
        } else {
            res = new Search(node, cts, i);

            if(height <= h){
                srchs[height] = res;
            }

            if(height == 0){
                return;
            }

            if(i < 0){
                i = -i -1;
            }

            node = (*cts->children)[i];
            --height;
        }
    }
}

Keys* difference(Keys* a, Key key){
    Keys* newArray = new Keys(a->length - 1);
    
    int i = 0;
    Key* src = a->elements;
    Key* dest = newArray->elements;

    while(i < a->length){
        if(*src != key){
            *dest = *src;
            ++dest;
        }
        
        ++i;
        ++src;
    }

    return newArray;
}

template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::remove(T value){
    Search* srch = traverseAndCleanup(value);

    Key key = special_hash(value);

    while(true){
        Node* node = srch->node;
        Contents* cts = srch->contents;
        if(srch->index < 0){
            return false;
        }

        Keys* items = difference(cts->items, key);
        Contents* update = new Contents(items, nullptr, cts->link);

        if(node->casContents(cts, update)){
            return true;
        }

        srch = moveForward(node, key);
    }
}

template<typename T, int Threads>
Search* MultiwaySearchTree<T, Threads>::traverseAndCleanup(T value){
    Key key = special_hash(value);

    Node* node = root->node;

    Contents* cts = node->contents;
    Keys* items = cts->items;
    int i = search(items, key);
    Key max = {KeyFlag::EMPTY, 0};

    while(cts->children){
        if(-i -1 == items->length){
            if(items->length > 0){
                max = (*items)[items->length - 1];
            }

            node = cleanLink(node, cts);
        } else {
            if (i < 0){
                i = -i -1;
            }

            cleanNode(key, node, cts, i, max);
            node = (*cts->children)[i];
            max = {KeyFlag::EMPTY, 0};
        }

        cts = node->contents;
        items = cts->items;
        i = search(items, key);
    }

    while(true){
        if(i > -cts->items->length - 1){
            return new Search(node, cts, i);
        }

        node = cleanLink(node, cts);
        cts = node->contents;
        i = search(cts->items, key);
    }
}

template<typename T, int Threads>
Node* MultiwaySearchTree<T, Threads>::cleanLink(Node* node, Contents* contents){
    while(true){
        Node* newLink = pushRight(contents->link, {KeyFlag::EMPTY, 0});

        if(newLink == contents->link){
            return contents->link;
        }

        Contents* update = new Contents(contents->items, contents->children, newLink);

        if(node->casContents(contents, update)){
            return update->link;
        }

        contents = node->contents;
    }
}

int compare(Key k1, Key k2){
    if(k1.flag == KeyFlag::INF){
        return 1;
    }
    
    if(k2.flag == KeyFlag::INF){
        return -1;
    }

    return k1.key - k2.key; //TODO Check if 1 - 2 or 2 - 1
}

template<typename T, int Threads>
void MultiwaySearchTree<T, Threads>::cleanNode(Key key, Node* node, Contents* contents, int index, Key leftBarrier){
    while(true){
        int length = contents->items->length;

        if(length == 0){
            return;
        } else if(length == 1){
            if(cleanNode1(node, contents, leftBarrier)){
                return;
            }
        } else if(length == 2){
            if(cleanNode2(node, contents, leftBarrier)){
                return;
            }
        } else {
            if(cleanNodeN(node, contents, index, leftBarrier)){
                return;
            }
        }

        contents = node->contents;
        index = search(contents->items, key);

        if(-index - 1 == contents->items->length){
            return;
        } else if(index < 0){
            index = -index -1;
        }
    }
}

bool cleanNode1(Node* node, Contents* contents, Key leftBarrier){
    bool success = attemptSlideKey(node, contents);

    if(success){
        return true;
    }

    Key key = (*contents->items)[0];

    if(leftBarrier.flag != KeyFlag::EMPTY && compare(key, leftBarrier) <= 0){
        leftBarrier = {KeyFlag::EMPTY, 0};
    }

    Node* childNode = (*contents->children)[0];
    Node* adjustedChild = pushRight(childNode, leftBarrier);

    if(adjustedChild == childNode){
        return true;
    }

    return shiftChild(node, contents, 0, adjustedChild);
}

bool cleanNode2(Node* node, Contents* contents, Key leftBarrier){
    bool success = attemptSlideKey(node, contents);

    if(success){
        return true;
    }

    Key key = (*contents->items)[0];

    if(leftBarrier.flag != KeyFlag::EMPTY && compare(key, leftBarrier) <= 0){
        leftBarrier = {KeyFlag::EMPTY, 0};
    }

    Node* childNode1 = (*contents->children)[0];
    Node* adjustedChild1 = pushRight(childNode1, leftBarrier);
    leftBarrier = (*contents->items)[0];
    Node* childNode2 = (*contents->children)[1];
    Node* adjustedChild2 = pushRight(childNode2, leftBarrier);

    if((adjustedChild1 == childNode1) && (adjustedChild2 == childNode2)){
        return true;
    }

    return shiftChildren(node, contents, adjustedChild1, adjustedChild2);
}

bool cleanNodeN(Node* node, Contents* contents, int index, Key leftBarrier){
    Key key0 = (*contents->items)[0];

    if(index > 0){
        leftBarrier = (*contents->items)[index - 1];
    } else if(leftBarrier.flag != KeyFlag::EMPTY && compare(key0, leftBarrier) <= 0){
        leftBarrier = {KeyFlag::EMPTY, 0};
    }

    Node* childNode = (*contents->children)[index];
    Node* adjustedChild = pushRight(childNode, leftBarrier);

    if(index == 0 || index == contents->children->length - 1){
        if(adjustedChild == childNode){
            return true;
        }

        return shiftChild(node, contents, index, adjustedChild);
    }

    Node* adjustedNeighbor = pushRight((*contents->children)[index + 1], (*contents->items)[index]);

    if(adjustedNeighbor == adjustedChild){
        return dropChild(node, contents, index, adjustedChild);
    } else if(adjustedChild != childNode){
        return shiftChild(node, contents, index, adjustedChild);
    } else {
        return true;
    }
}

Node* pushRight(Node* node, Key leftBarrier){
    while(true){
        Contents* contents = node->contents;

        int length = contents->items->length;

        if(length == 0){
            node = contents->link;
        } else if(leftBarrier.flag == KeyFlag::EMPTY || compare((*contents->items)[length - 1], leftBarrier) > 0){
            return node;
        } else {
            node = contents->link;
        }
    }
}

#define MAX_HEIGHT 10

//TODO Put the generator at the class level
template<typename T, int Threads>
int MultiwaySearchTree<T, Threads>::randomLevel(){
    static std::mt19937 gen;
    std::geometric_distribution<int> dist(1.0 - 1.0 / 32.0);
    
    //TODO If rand > MAX_HEIGHT return MAX_LEVEL
    int x;
    do{
        x = dist(gen);
    } while (x > MAX_HEIGHT);

    return x;
}

template<typename T, int Threads>
int MultiwaySearchTree<T, Threads>::search(Keys* items, Key key){
    int low = 0;
    int high = items->length - 1;

    if(low > high){
        return -1;
    }

    if((*items)[high].flag == KeyFlag::INF){
        high--;
    }

    while(low <= high){
        int mid = (low + high) >> 1; //TODO check
        Key midVal = (*items)[mid];
        int cmp = compare(key, midVal);

        if(cmp > 0){
            low = mid + 1;
        } else if(cmp < 0){
            high = mid - 1;
        } else {
            return mid;
        }
    }
    
    return -(low + 1); //not found
}

template<typename T, int Threads>
HeadNode* MultiwaySearchTree<T, Threads>::increaseRootHeight(int target){
    HeadNode* root = this->root;
    int height = root->height;

    while(height < target){
        Keys* keys = new Keys(1);
        (*keys)[0].flag = KeyFlag::INF;
        Children* children = new Children(1); 
        (*children)[0] = root->node;
        Contents* contents = new Contents(keys, children, nullptr);
        Node* newNode = new Node(contents);
        HeadNode* update = new HeadNode(newNode, height + 1);
        CASPTR(&this->root, root, update);
        root = this->root;
        height = root->height;
    }

    return root;
}

template<typename T, int Threads>
Search* MultiwaySearchTree<T, Threads>::moveForward(Node* node, Key key){
    while(true){
        Contents* contents = node->contents;

        int index = search(contents->items, key);

        if(index > -contents->items->length - 1){
            return new Search(node, contents, index);
        } else {
            node = contents->link;
        }
    }
}

Children* copyChildren(Children* rhs){
    Children* copy = new Children(rhs->length);

    for(int i = 0; i < rhs->length; ++i){
        (*copy)[i] = (*rhs)[i];
    }

    return copy;
}

bool shiftChild(Node* node, Contents* contents, int index, Node* adjustedChild){
    Children* newChildren = copyChildren(contents->children);
    (*newChildren)[index] = adjustedChild;

    Contents* update = new Contents(contents->items, newChildren, contents->link);
    return node->casContents(contents, update);
}

bool shiftChildren(Node* node, Contents* contents, Node* child1, Node* child2){
    Children* newChildren = new Children(2);
    (*newChildren)[0] = child1;
    (*newChildren)[0] = child2;

    Contents* update = new Contents(contents->items, newChildren, contents->link);
    return node->casContents(contents, update);
}

bool dropChild(Node* node, Contents* contents, int index, Node* adjustedChild){
    int length = contents->items->length;

    Keys* newKeys = new Keys(length - 1);
    Children* newChildren = new Children(length - 1);

    for(int i = 0; i < index; ++i){
        (*newKeys)[i] = (*contents->items)[i];
        (*newChildren)[i] = (*contents->children)[i];
    }
    
    (*newChildren)[index] = adjustedChild;
    
    for(int i = index + 1; i < length; ++i){
        (*newKeys)[i - 1] = (*contents->items)[i];
    }

    for(int i = index + 2; i < length; ++i){
        (*newChildren)[i - 1] = (*contents->children)[i];
    }

    Contents* update = new Contents(newKeys, newChildren, contents->link);
    return node->casContents(contents, update);
}

} //end of lfmst

#endif
