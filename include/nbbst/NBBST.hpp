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

Info* getRef(Update update){
   CONVERSION<Info> conversion;
   conversion.node = update;
   conversion.value &= (~0l - 3); //Clear the last two bits

   return conversion.node;
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
    //std::cout << "CompareAndSet" << std::endl;

    CONVERSION<Info> current;
    current.node = ref;
    setState(current, state);

    CONVERSION<Info> next;
    next.node = newRef;
    setState(next, newState);
   
    return CASPTR(ptr, current.node, next.node); 
}

/* struct Update {
    Info* info;
    UpdateState state;
};*/

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
};

struct DInfo : public Info {
    Internal* gp;
    Internal* p;
    Leaf* l;
    Update pupdate;
};

struct SearchResult {
    Internal* gp;
    Internal* p;
    Leaf* l;
    Update pupdate;
    Update gpupdate;
};

class NBBST {
    public:
        NBBST();

        bool contains(int value); //TODO Change to T value
        bool add(int value); //TODO Change to T value
        bool remove(int value); //TODO Change to T value

    private:
        SearchResult Search(int key); //TODO Change to T value       
        void HelpInsert(IInfo* op);
        bool HelpDelete(DInfo* op);
        void HelpMarked(DInfo* op);
        void Help(Update u);
        void CASChild(Internal* parent, Node* old, Node* newNode);

        Internal* root;
};

NBBST::NBBST(){
    root = new Internal();
    root->key = INT_MAX;//TODO Check int_max
    set(&root->update, nullptr, CLEAN);

    root->left = new Leaf(INT_MIN);
    root->right = new Leaf(INT_MAX);
}

SearchResult NBBST::Search(int key){
    //std::cout << "search " << key << std::endl;

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

bool NBBST::contains(int value){
    //std::cout << "contains " << value << std::endl;

    int key = hash(value);

    SearchResult result = Search(key);

    return result.l->key == key;
}

bool NBBST::add(int value){
    ////std::cout << "add " << value << std::endl;

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

            IInfo* op = new IInfo();
            op->p = p;
            op->l = l;
            op->newInternal = newInternal;

            Update result = p->update;
            if(CompareAndSet(&p->update, getRef(pupdate), getRef(op), getState(pupdate), IFLAG)){
                HelpInsert(op);
                return true;
            } else {
                Help(result);
            }
        }
    }
}

void NBBST::HelpInsert(IInfo* op){
    //std::cout << "help insert " << std::endl;

    CASChild(op->p, op->l, op->newInternal);
    CompareAndSet(&op->p->update, getRef(op), getRef(op), IFLAG, CLEAN);
}

bool NBBST::remove(int value){
    //std::cout << "remove " << value << std::endl;

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
            DInfo* op = new DInfo();
            op->gp = gp;
            op->p = p;
            op->l = l;
            op->pupdate = pupdate;

            Update result = gp->update;
            if(CompareAndSet(&gp->update, getRef(gpupdate), getRef(op), getState(gpupdate), DFLAG)){
                if(HelpDelete(op)){
                    return true;
                }
            } else {
                Help(result);
            }
        }
    }
}

bool NBBST::HelpDelete(DInfo* op){
    //std::cout << "help delete " << std::endl;

    Update result = op->p->update;

    //If we succeed or if another has succeeded for us
    if(CompareAndSet(&op->p->update, getRef(op->pupdate), getRef(op), getState(op->pupdate), MARK) || (getState(op->p->update) == MARK && getRef(op->p->update) == getRef(op))){
        HelpMarked(static_cast<DInfo*>(getRef(op)));
        return true;
    } else {
        Help(result);
        CompareAndSet(&op->gp->update, getRef(op), getRef(op), DFLAG, CLEAN);
        return false;
    }
}

void NBBST::HelpMarked(DInfo* op){
    //std::cout << "help marked" << std::endl;

    Node* other;

    if(op->p->right == op->l){
        other = op->p->left;
    } else {
        other = op->p->right;
    }

    CASChild(op->gp, op->p, other);

    CompareAndSet(&op->gp->update, getRef(op), getRef(op), DFLAG, CLEAN);
}

void NBBST::Help(Update u){
    //std::cout << "help" << std::endl;

    if(getState(u) == IFLAG){
        HelpInsert(static_cast<IInfo*>(getRef(u)));
    } else if(getState(u) == MARK){
        HelpMarked(static_cast<DInfo*>(getRef(u)));
    } else if(getState(u) == DFLAG){
        HelpDelete(static_cast<DInfo*>(getRef(u)));
    }
}
        
void NBBST::CASChild(Internal* parent, Node* old, Node* newNode){
    //std::cout << "cas child" << std::endl;

    if(newNode->key < parent->key){
        CASPTR(&parent->left, old, newNode);
    } else {
        CASPTR(&parent->right, old, newNode);
    }
}

}
