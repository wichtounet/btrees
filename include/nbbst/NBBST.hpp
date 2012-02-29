#include "hash.hpp"
#include "Utils.hpp"

namespace nbbst {
    
enum UpdateState {
    CLEAN,
    DFLAG,
    IFLAG,
    MARK
};

struct Info {

};

typedef Info* Update; 

/* struct Update {
    Info* info;
    UpdateState state;
};*/

UpdateState getState(Update update){
   CONVERSION<Info> conversion;
   conversion.node = update;
   conversion.value &= 3l; //Clear all the bits except the last two bits
   
   if(conversion.value == 0){
        return CLEAN;   
   } else if(conversion.value == 1){
        return DFLAG;
   } else if(conversion.value == 2){
        return IFLAG;
   } else if(conversion.value == 3){
        return MARK;
   }

   assert(false);
}

inline Update Unmark(Update info){
    return reinterpret_cast<Update>(reinterpret_cast<unsigned long>(info) & (~0l - 3));
}

template<typename T>
void setState(CONVERSION<T>& conversion, UpdateState state){
    conversion.value &= (~0l - 3); //Clear the last two bits

    if(state == CLEAN){
       conversion.value |= 0; 
    } else if(state == DFLAG){
       conversion.value |= 1; 
    } else if(state == IFLAG){
       conversion.value |= 2; 
    } else if(state == MARK){
       conversion.value |= 3; 
    }
}

void set(Update* update, Info* info, UpdateState state){
    CONVERSION<Info> conversion;
    conversion.node = info;
    setState(conversion, state);

    *update = conversion.node;
}

bool CompareAndSet(Update* ptr, Info* ref, Info* newRef, UpdateState state, UpdateState newState){
    CONVERSION<Info> current;
    current.node = ref;
    setState(current, state);

    CONVERSION<Info> next;
    next.node = newRef;
    setState(next, newState);
   
    return CASPTR(ptr, current.node, next.node); 
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
};

template<typename T>
class NBBST {
    public:
        NBBST();

        bool contains(T value);
        bool add(T value);
        bool remove(T value);

    private:
        SearchResult Search(int key);      
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
    set(&root->update, nullptr, CLEAN);

    root->left = new Leaf(INT_MIN);
    root->right = new Leaf(INT_MAX);
}

template<typename T>
SearchResult NBBST<T>::Search(int key){
    Internal* gp;
    Internal* p;
    Node* l = root;
    Update gpupdate;
    Update pupdate;

    while(l->internal){
        gp = p;
        p = static_cast<Internal*>(l);
        gpupdate = pupdate;
        pupdate = p->update;

        if(key < l->key){
            l = p->left;
        } else {
            l = p->right;
        }
    }

    return {gp, p, (Leaf*) l, pupdate, gpupdate};
}

template<typename T>
bool NBBST<T>::contains(T value){
    int key = hash(value);

    SearchResult result = Search(key);

    return result.l->key == key;
}

template<typename T>
bool NBBST<T>::add(T value){
    int key = hash(value);

    Leaf* newLeaf = new Leaf(key);

    while(true){
        SearchResult search = Search(key);

        Internal* p = search.p;
        Leaf* l = search.l;
        Update pupdate = search.pupdate;

        if(l->key == key){
            return false; //Key already in the set
        }

        if(getState(pupdate) != CLEAN){
            Help(pupdate);
        } else {
            Leaf* newSibling = new Leaf(l->key);
            Internal* newInternal = new Internal(std::max(key, l->key));
            set(&newInternal->update, nullptr, CLEAN);
            
            //Put the smaller child on the left
            if(newLeaf->key <= newSibling->key){
                newInternal->left = newLeaf;
                newInternal->right = newSibling;
            } else {
                newInternal->left = newSibling;
                newInternal->right = newLeaf;
            }

            IInfo* op = new IInfo(p, newInternal, l);

            Update result = p->update;
            if(CompareAndSet(&p->update, Unmark(pupdate), Unmark(op), getState(pupdate), IFLAG)){
                HelpInsert(op);
                return true;
            } else {
                Help(result);
            }
        }
    }
}

template<typename T>
void NBBST<T>::HelpInsert(IInfo* op){
    CASChild(op->p, op->l, op->newInternal);
    CompareAndSet(&op->p->update, Unmark(op), Unmark(op), IFLAG, CLEAN);
}

template<typename T>
bool NBBST<T>::remove(T value){
    int key = hash(value);

    while(true){
        SearchResult search = Search(key);
        Internal* gp = search.gp;
        Internal* p = search.p;
        Leaf* l = search.l;
        Update pupdate = search.pupdate;
        Update gpupdate = search.gpupdate;
        
        if(l->key != key){
            return false;
        }

        if(getState(gpupdate) != CLEAN){
            Help(gpupdate);
        } else if(getState(pupdate) != CLEAN){
            Help(pupdate);
        } else {
            DInfo* op = new DInfo(gp, p, l, pupdate);

            Update result = gp->update;
            if(CompareAndSet(&gp->update, Unmark(gpupdate), Unmark(op), getState(gpupdate), DFLAG)){
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
    if(CompareAndSet(&op->p->update, Unmark(op->pupdate), Unmark(op), getState(op->pupdate), MARK) || (getState(op->p->update) == MARK && Unmark(op->p->update) == Unmark(op))){
        HelpMarked(static_cast<DInfo*>(Unmark(op)));
        return true;
    } else {
        Help(result);
        CompareAndSet(&op->gp->update, Unmark(op), Unmark(op), DFLAG, CLEAN);
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

    CompareAndSet(&op->gp->update, Unmark(op), Unmark(op), DFLAG, CLEAN);
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
