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
    
    Update update;
    Node* left;
    Node* right;

    Node(bool internal, int key = 0) : internal(internal), key(key), update(nullptr), left(nullptr), right(nullptr) {};
};

Node* newInternal(int key = 0){
    return new Node(true, key); 
}

Node* newLeaf(int key){
    return new Node(false, key);
}

struct IInfo : public Info {
    Node* p;                //Internal
    Node* newInternal;      //Internal
    Node* l;                //Leaf

    IInfo(Node* p, Node* newInternal, Node* l) : p(p), newInternal(newInternal), l(l) {}
};

struct DInfo : public Info {
    Node* gp;       //Internal
    Node* p;        //Internal
    Node* l;        //Leaf
    Update pupdate;

    DInfo(Node* gp, Node* p, Node* l, Update pupdate) : gp(gp), p(p), l(l), pupdate(pupdate) {}
};

struct SearchResult {
    Node* gp;       //Internal
    Node* p;        //Internal
    Node* l;        //Leaf
    Update pupdate;
    Update gpupdate;

    SearchResult() : gp(nullptr), p(nullptr), l(nullptr), pupdate(nullptr), gpupdate(nullptr) {}
};

template<typename T, int Threads>
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
        void CASChild(Node* parent, Node* old, Node* newNode);

        Node* root;
};

template<typename T, int Threads>
NBBST<T, Threads>::NBBST(){
    root = newInternal(INT_MAX);//TODO Check int_max
    root->update = Mark(nullptr, CLEAN);

    root->left = newLeaf(INT_MIN);
    root->right = newLeaf(INT_MAX);
}

template<typename T, int Threads>
void NBBST<T, Threads>::Search(int key, SearchResult* result){
    Node* l = root;

    while(l->internal){
        result->gp = result->p;
        result->p = l;
        result->gpupdate = result->pupdate;
        result->pupdate = result->p->update;

        if(key < l->key){
            l = result->p->left;
        } else {
            l = result->p->right;
        }
    }

    result->l = l;
}

template<typename T, int Threads>
bool NBBST<T, Threads>::contains(T value){
    int key = hash(value);

    SearchResult result;
    Search(key, &result);

    return result.l->key == key;
}

template<typename T, int Threads>
bool NBBST<T, Threads>::add(T value){
    int key = hash(value);

    Node* newNode = newLeaf(key);

    SearchResult search;

    while(true){
        Search(key, &search);

        if(search.l->key == key){
            delete newNode;

            return false; //Key already in the set
        }

        if(getState(search.pupdate) != CLEAN){
            Help(search.pupdate);
        } else {
            Node* newSibling = newLeaf(search.l->key);
            Node* newInt = newInternal(std::max(key, search.l->key));
            newInt->update = Mark(nullptr, CLEAN);
            
            //Put the smaller child on the left
            if(newNode->key <= newSibling->key){
                newInt->left = newNode;
                newInt->right = newSibling;
            } else {
                newInt->left = newSibling;
                newInt->right = newNode;
            }

            IInfo* op = new IInfo(search.p, newInt, search.l);

            Update result = search.p->update;
            if(CASPTR(&search.p->update, search.pupdate, Mark(op, IFLAG))){
                HelpInsert(op);
                return true;
            } else {
                delete newSibling;
                delete newInt;
                delete op;

                Help(result);
            }
        }
    }
}

template<typename T, int Threads>
void NBBST<T, Threads>::HelpInsert(IInfo* op){
    CASChild(op->p, op->l, op->newInternal);
    CASPTR(&op->p->update, Mark(op, IFLAG), Mark(op, CLEAN));
}

template<typename T, int Threads>
bool NBBST<T, Threads>::remove(T value){
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

template<typename T, int Threads>
bool NBBST<T, Threads>::HelpDelete(DInfo* op){
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

template<typename T, int Threads>
void NBBST<T, Threads>::HelpMarked(DInfo* op){
    Node* other;

    if(op->p->right == op->l){
        other = op->p->left;
    } else {
        other = op->p->right;
    }

    CASChild(op->gp, op->p, other);

    CASPTR(&op->gp->update, Mark(op, DFLAG), Mark(op, CLEAN));
}

template<typename T, int Threads>
void NBBST<T, Threads>::Help(Update u){
    if(getState(u) == IFLAG){
        HelpInsert(static_cast<IInfo*>(Unmark(u)));
    } else if(getState(u) == MARK){
        HelpMarked(static_cast<DInfo*>(Unmark(u)));
    } else if(getState(u) == DFLAG){
        HelpDelete(static_cast<DInfo*>(Unmark(u)));
    }
}
        
template<typename T, int Threads>
void NBBST<T, Threads>::CASChild(Node* parent, Node* old, Node* newNode){
    if(newNode->key < parent->key){
        CASPTR(&parent->left, old, newNode);
    } else {
        CASPTR(&parent->right, old, newNode);
    }
}

}
