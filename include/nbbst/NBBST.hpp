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
   conversion.value &= 3; //Clear all the bits except the last two bits
   
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

void set(Update* update, Info* info, UpdateState state){
    CONVERSION<Info> conversion;
    conversion.node = info;
    conversion.value &= (~0 - 3); //Clear the last two bits

    if(state == CLEAN){
       conversion.value |= 0; 
    } else if(state == DFLAG){
       conversion.value |= 1; 
    } else if(state == IFLAG){
       conversion.value |= 2; 
    } else if(state == MARK){
       conversion.value |= 3; 
    }

    *update = conversion.node;
}

/* struct Update {
    Info* info;
    UpdateState state;
};*/

struct Node {
    bool internal;
    unsigned int key;

    Node(bool internal) : internal(internal) {};
    Node(bool internal, unsigned int key) : internal(internal), key(key) {};
};

struct Internal : public Node {
    Update update;
    Node* left;
    Node* right;

    Internal() : Node(true) {}
    Internal(unsigned int key) : Node(true, key) {}
};

struct Leaf : public Node {
    Leaf(unsigned int key) : Node(false, key) {}
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
        SearchResult Search(int value); //TODO Change to T value       
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

    Leaf* left = new Leaf(INT_MAX);
    root->left = left;
    
    Leaf* right = new Leaf(INT_MAX);
    root->right = right;
}

SearchResult NBBST::Search(int value){
    unsigned int key = hash(value);

    Internal* gp;
    Internal* p;
    Node* l = root;
    Update gpupdate;
    Update pupdate;

    while(l->internal){
        gp = p;
        p = (Internal*) l;
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
    unsigned int key = hash(value);

    SearchResult result = Search(value);

    return result.l->key == key;
}

bool NBBST::add(int value){
    unsigned int key = hash(value);

    Internal* p;
    Internal* newInternal;
    Leaf* l;
    Leaf* newSibling;
    Leaf* newLeaf = new Leaf(key);
    Update pupdate;
    Update result;
    IInfo* op;

    while(true){
        SearchResult search = Search(value);

        p = search.p;
        l = search.l;
        pupdate = search.pupdate;

        if(l->key == key){
            return false; //Key already in the set
        }

        if(getState(pupdate) != CLEAN){
            Help(pupdate);
        } else {
            newSibling = new Leaf(l->key);
            newInternal = new Internal(std::max(key, l->key));
            set(&newInternal->update, nullptr, CLEAN);
            
            //Put the smaller child on the left
            if(newLeaf->key <= newSibling->key){
                newInternal->left = newLeaf;
                newInternal->right = newSibling;
            } else {
                newInternal->left = newSibling;
                newInternal->right = newLeaf;
            }

            op = new IInfo();
            op->p = p;
            op->l = l;
            op->newInternal = newInternal;

            bool cas = true; //TODO
            if(cas){
                HelpInsert(op);
                return true;
            } else {
                Help(result); //TODO result = old value before CAS
            }
        }
    }
}

void NBBST::HelpInsert(IInfo* op){
    //CAS-Child(op->p,op-l, op->newInternal)
    //CAS(op->p->update, (iflag,op), (clean,op))
}

bool NBBST::remove(int value){
    unsigned int key = hash(value);

    Internal* gp;
    Internal* p;
    Leaf* l;
    Update pupdate;
    Update gpupdate;
    Update result;
    DInfo* op;

    while(true){
        SearchResult search = Search(value);
        gp = search.gp;
        p = search.p;
        l = search.l;
        pupdate = search.pupdate;
        gpupdate = search.gpupdate;
        
        if(l->key != key){
            return false;
        }

        if(getState(gpupdate) != CLEAN){
            Help(gpupdate);
        } else if(getState(pupdate) != CLEAN){
            Help(pupdate);
        } else {
            op = new DInfo();
            op->gp = gp;
            op->p = p;
            op->l = l;
            op->pupdate = pupdate;

            //CAS SHIT
        }
    }
}

bool NBBST::HelpDelete(DInfo* op){
    Update result;
    //CAS

}

void NBBST::HelpMarked(DInfo* op){
    Node* other;

    if(op->p->right == op->l){
        other = op->p->left;
    } else {
        other = op->p->right;
    }

    CASChild(op->gp, op->p, other);

    //TODO CAS(op->gp->update, (DFLAG, op), (CLEAN,op))
}

void NBBST::Help(Update u){
    if(u.state == IFLAG){
        HelpInsert((IInfo*) u.info);
    } else if(u.state == MARK){
        HelpMarked((DInfo*) u.info);
    } else if(u.state == DFLAG){
        HelpDelete((DInfo*) u.info);
    }
}
        
void NBBST::CASChild(Internal* parent, Node* old, Node* newNode){
    if(newNode->key < parent->key){
        CASPTR(&parent->left, old, newNode);
    } else {
        CASPTR(&parent->right, old, newNode);
    }
}

}
