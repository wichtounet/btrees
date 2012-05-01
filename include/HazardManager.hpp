#ifndef HAZARD_MANAGER
#define HAZARD_MANAGER

#include <cassert>

//Thread local id
//Note: __thread is GCC specific
extern __thread unsigned int thread_num;

#include <list>
#include <array>
#include <algorithm>
#include <iostream>

template<typename Node, unsigned int Threads, unsigned int Size = 2, unsigned int Prefill = 50>
class HazardManager {
    public:
        HazardManager();
        ~HazardManager();

        HazardManager(const HazardManager& rhs) = delete;
        HazardManager& operator=(const HazardManager& rhs) = delete;

        /* Manage nodes */
        void releaseNode(Node* node);
        void safe_release_node(Node* node);
        Node* getFreeNode();

        /* Manage references  */
        void publish(Node* node, unsigned int i);
        void release(unsigned int i);
        void releaseAll();

    private:
        std::array<std::array<Node*, Size>, Threads> Pointers;
        std::array<std::list<Node*>, Threads> LocalQueues;
        std::array<std::list<Node*>, Threads> FreeQueues;
        
        bool isReferenced(Node* node);

        /* Verify the template parameters */
        static_assert(Threads > 0, "The number of threads must be greater than 0");
        static_assert(Size > 0, "The number of hazard pointers must greater than 0");
};

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
HazardManager<Node, Threads, Size, Prefill>::HazardManager(){
    for(unsigned int tid = 0; tid < Threads; ++tid){
        for(unsigned int j = 0; j < Size; ++j){
            Pointers.at(tid).at(j) = nullptr;
        }

        if(Prefill > 0){
            for(unsigned int i = 0; i < Prefill; i++){
                FreeQueues.at(tid).push_back(new Node());
            }
        } 
    }
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
HazardManager<Node, Threads, Size, Prefill>::~HazardManager(){
    for(unsigned int tid = 0; tid < Threads; ++tid){
        //No need to delete Hazard Pointers because each thread need to release its published references

        while(!LocalQueues.at(tid).empty()){
            delete LocalQueues.at(tid).front();
            LocalQueues.at(tid).pop_front();
        }
        
        while(!FreeQueues.at(tid).empty()){
            delete FreeQueues.at(tid).front();
            FreeQueues.at(tid).pop_front();
        }
    }
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::safe_release_node(Node* node){
    //If the node is null, we have nothing to do
    if(node){
        if(std::find(LocalQueues.at(thread_num).begin(), LocalQueues.at(thread_num).end(), node) != LocalQueues.at(thread_num).end()){
            return;
        }

        //Add the node to the localqueue
        LocalQueues.at(thread_num).push_back(node);
    }
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::releaseNode(Node* node){
    //If the node is null, we have nothing to do
    if(node){
 /*       //TODO Remove that test after debugging
        if(std::find(LocalQueues.at(thread_num).begin(), LocalQueues.at(thread_num).end(), node) != LocalQueues.at(thread_num).end()){
            std::cout << node << std::endl;

            //    assert(false);
            return;
        }*/

        //Add the node to the localqueue
        LocalQueues.at(thread_num).push_back(node);
    }
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
Node* HazardManager<Node, Threads, Size, Prefill>::getFreeNode(){
    int tid = thread_num;

    //First, try to get a free node from the free queue
    if(!FreeQueues.at(tid).empty()){
        Node* free = FreeQueues.at(tid).front();
        FreeQueues.at(tid).pop_front();

        return free;
    }

    //If there are enough local nodes, move then to the free queue
    if(LocalQueues.at(tid).size() > (Size + 1) * Threads){
        typename std::list<Node*>::iterator it = LocalQueues.at(tid).begin();
        typename std::list<Node*>::iterator end = LocalQueues.at(tid).end();

        while(it != end){
            if(!isReferenced(*it)){
                FreeQueues.at(tid).push_back(*it);

                it = LocalQueues.at(tid).erase(it);
            }
            else {
                ++it;
            }
        }
        
        Node* free = FreeQueues.at(tid).front();
        FreeQueues.at(tid).pop_front();

        return free;
    }

    //There was no way to get a free node, allocate a new one
    return new Node();
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
bool HazardManager<Node, Threads, Size, Prefill>::isReferenced(Node* node){
    for(unsigned int tid = 0; tid < Threads; ++tid){
        for(unsigned int i = 0; i < Size; ++i){
            if(Pointers.at(tid).at(i) == node){
                return true;
            }
        }
    }

    return false;
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::publish(Node* node, unsigned int i){
    Pointers.at(thread_num).at(i) = node;
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::release(unsigned int i){
    Pointers.at(thread_num).at(i) = nullptr;
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::releaseAll(){
    for(unsigned int i = 0; i < Size; ++i){
        Pointers.at(thread_num).at(i) = nullptr;
    }
}

#endif
