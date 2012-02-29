#include "hash.hpp"
#include "Utils.hpp"

namespace nbbst {
    
enum UpdateState {
    CLEAN = 0,
    DFLAG = 1,
    IFLAG = 2,
    MARK  = 3
};

struct Info {

};

typedef Info* Update; 

/* 
Update is equivalent to the following struct:

struct Update {
    Info* info;
    UpdateState state;
};

but stored inside a single word.

*/

inline UpdateState getState(Update update){
   return static_cast<UpdateState>(reinterpret_cast<unsigned long>(update) & 3l);
}

inline Update Unmark(Update info){
    return reinterpret_cast<Update>(reinterpret_cast<unsigned long>(info) & (~0l - 3));
}

inline Update Mark(Update info, UpdateState state){
    return reinterpret_cast<Update>((reinterpret_cast<unsigned long>(info) & (~0l - 3)) | static_cast<unsigned int>(state));
}

struct Node {
    bool internal;
    int key;

    Node(bool internal) : internal(internal) {};
    Node(bool internal, int key) : internal(internal), key(key) {};
};

struct Internal : public Node {
    Update update;
    Node* left;
    Node* right;

    Internal() : Node(true) {}
    Internal(int key) : Node(true, key) {}
};

struct Leaf : public Node {
    Leaf(int key) : Node(false, key) {}
};

struct IInfo : public Info {
    Internal* p;
    Internal* newInternal;
    Leaf* l;

    IInfo(Internal* p, Internal* newInternal, Leaf* l) : p(p), newInternal(newInternal), l(l) {}
};

struct DInfo : public Info {
    Internal* gp;
    Internal* p;
    Leaf* l;
    Update pupdate;

    DInfo(Internal* gp, Internal* p, Leaf* l, Update pupdate) : gp(gp), p(p), l(l), pupdate(pupdate) {}
};

struct SearchResult {
    Internal* gp;
    Internal* p;
    Leaf* l;
    Update pupdate;
    Update gpupdate;

    SearchResult() : gp(nullptr), p(nullptr), l(nullptr), pupdate(nullptr), gpupdate(nullptr) {}
};

template<typename T>
class NBBST {
    public:
        NBBST();

        bool contains(T value);
        bool add(T value);
        bool remove(T value);

    private:
        void Search(int key, SearchResult* result);      
        void HelpInsert(IInfo* op);
        bool HelpDelete(DInfo* op);
        void HelpMarked(DInfo* op);
        void Help(Update u);
        void CASChild(Internal* parent, Node* old, Node* newNode);

        Internal* root;
};

template<typename T>
NBBST<T>::NBBST(){
    root = new Internal();
    root->key = INT_MAX;//TODO Check int_max
    root->update = Mark(nullptr, CLEAN);

    root->left = new Leaf(INT_MIN);
    root->right = new Leaf(INT_MAX);
}

template<typename T>
void NBBST<T>::Search(int key, SearchResult* result){
    Node* l = root;

    while(l->internal){
        result->gp = result->p;
        result->p = static_cast<Internal*>(l);
        result->gpupdate = result->pupdate;
        result->pupdate = result->p->update;

        if(key < l->key){
            l = result->p->left;
        } else {
            l = result->p->right;
        }
    }

    result->l = static_cast<Leaf*>(l);
}

template<typename T>
bool NBBST<T>::contains(T value){
    int key = hash(value);

    SearchResult result;
    Search(key, &result);

    return result.l->key == key;
}

template<typename T>
bool NBBST<T>::add(T value){
    int key = hash(value);

    Leaf* newLeaf = new Leaf(key);

    SearchResult search;

    while(true){
        Search(key, &search);

        if(search.l->key == key){
            delete newLeaf;

            return false; //Key already in the set
        }

        if(getState(search.pupdate) != CLEAN){
            Help(search.pupdate);
        } else {
            Leaf* newSibling = new Leaf(search.l->key);
            Internal* newInternal = new Internal(std::max(key, search.l->key));
            newInternal->update = Mark(nullptr, CLEAN);
            
            //Put the smaller child on the left
            if(newLeaf->key <= newSibling->key){
                newInternal->left = newLeaf;
                newInternal->right = newSibling;
            } else {
                newInternal->left = newSibling;
                newInternal->right = newLeaf;
            }

            IInfo* op = new IInfo(search.p, newInternal, search.l);

            Update result = search.p->update;
            if(CASPTR(&search.p->update, search.pupdate, Mark(op, IFLAG))){
                HelpInsert(op);
                return true;
            } else {
                delete newSibling;
                delete newInternal;
                delete op;

                Help(result);
            }
        }
    }
}

template<typename T>
void NBBST<T>::HelpInsert(IInfo* op){
    CASChild(op->p, op->l, op->newInternal);
    CASPTR(&op->p->update, Mark(op, IFLAG), Mark(op, CLEAN));
}

template<typename T>
bool NBBST<T>::remove(T value){
    int key = hash(value);

    SearchResult search;

    while(true){
        Search(key, &search);
        
        if(search.l->key != key){
            return false;
        }

        if(getState(search.gpupdate) != CLEAN){
            Help(search.gpupdate);
        } else if(getState(search.pupdate) != CLEAN){
            Help(search.pupdate);
        } else {
            DInfo* op = new DInfo(search.gp, search.p, search.l, search.pupdate);

            Update result = search.gp->update;
            if(CASPTR(&search.gp->update, search.gpupdate, Mark(op, DFLAG))){
                if(HelpDelete(op)){
                    return true;
                }
            } else {
                Help(result);
            }
        }
    }
}

template<typename T>
bool NBBST<T>::HelpDelete(DInfo* op){
    Update result = op->p->update;

    //If we succeed or if another has succeeded for us
    if(CASPTR(&op->p->update, op->pupdate, Mark(op, MARK)) || (getState(op->p->update) == MARK && Unmark(op->p->update) == Unmark(op))){
        HelpMarked(static_cast<DInfo*>(Unmark(op)));
        return true;
    } else {
        Help(result);
        CASPTR(&op->gp->update, Mark(op, DFLAG), Mark(op, CLEAN));
        return false;
    }
}

template<typename T>
void NBBST<T>::HelpMarked(DInfo* op){
    Node* other;

    if(op->p->right == op->l){
        other = op->p->left;
    } else {
        other = op->p->right;
    }

    CASChild(op->gp, op->p, other);

    CASPTR(&op->gp->update, Mark(op, DFLAG), Mark(op, CLEAN));
}

template<typename T>
void NBBST<T>::Help(Update u){
    if(getState(u) == IFLAG){
        HelpInsert(static_cast<IInfo*>(Unmark(u)));
    } else if(getState(u) == MARK){
        HelpMarked(static_cast<DInfo*>(Unmark(u)));
    } else if(getState(u) == DFLAG){
        HelpDelete(static_cast<DInfo*>(Unmark(u)));
    }
}
        
template<typename T>
void NBBST<T>::CASChild(Internal* parent, Node* old, Node* newNode){
    if(newNode->key < parent->key){
        CASPTR(&parent->left, old, newNode);
    } else {
        CASPTR(&parent->right, old, newNode);
    }
}

}
