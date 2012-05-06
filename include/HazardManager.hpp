#ifndef HAZARD_MANAGER
#define HAZARD_MANAGER

#include <cassert>

#define DEBUG //Indicates that the at() function is used for array accesses

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

        std::list<Node*>& direct_free(unsigned int t);
        std::list<Node*>& direct_local(unsigned int t);

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
#ifdef DEBUG
            Pointers.at(tid).at(j) = nullptr;
#else
            Pointers[tid][j] = nullptr;
#endif
        }

        if(Prefill > 0){
            for(unsigned int i = 0; i < Prefill; i++){
#ifdef DEBUG
                FreeQueues.at(tid).push_back(new Node());
#else
                FreeQueues[tid].push_back(new Node());
#endif
            }
        } 
    }
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
HazardManager<Node, Threads, Size, Prefill>::~HazardManager(){
    for(unsigned int tid = 0; tid < Threads; ++tid){
        //No need to delete Hazard Pointers because each thread need to release its published references

#ifdef DEBUG
        while(!LocalQueues.at(tid).empty()){
            delete LocalQueues.at(tid).front();
            LocalQueues.at(tid).pop_front();
        }
        
        while(!FreeQueues.at(tid).empty()){
            delete FreeQueues.at(tid).front();
            FreeQueues.at(tid).pop_front();
        }
#else
        while(!LocalQueues[tid].empty()){
            delete LocalQueues[tid].front();
            LocalQueues[tid].pop_front();
        }
        
        while(!FreeQueues[tid].empty()){
            delete FreeQueues[tid].front();
            FreeQueues[tid].pop_front();
        }
#endif
    }
}
        
template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
std::list<Node*>& HazardManager<Node, Threads, Size, Prefill>::direct_free(unsigned int t){
#ifdef DEBUG
    return FreeQueues.at(t);
#else
    return FreeQueues[t];
#endif
}
        
template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
std::list<Node*>& HazardManager<Node, Threads, Size, Prefill>::direct_local(unsigned int t){
#ifdef DEBUG
    return LocalQueues.at(t);
#else
    return LocalQueues[t];
#endif
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
#ifdef DEBUG
        /*if(std::find(LocalQueues.at(thread_num).begin(), LocalQueues.at(thread_num).end(), node) != LocalQueues.at(thread_num).end()){
            std::cout << node << std::endl;
            return;
        }*/

        //Add the node to the localqueue
        LocalQueues.at(thread_num).push_back(node);
#else
        //Add the node to the localqueue
        LocalQueues[thread_num].push_back(node);
#endif
    }
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
Node* HazardManager<Node, Threads, Size, Prefill>::getFreeNode(){
    int tid = thread_num;

#ifdef DEBUG
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
#else
    //First, try to get a free node from the free queue
    if(!FreeQueues[tid].empty()){
        Node* free = FreeQueues[tid].front();
        FreeQueues[tid].pop_front();

        return free;
    }

    //If there are enough local nodes, move then to the free queue
    if(LocalQueues[tid].size() > (Size + 1) * Threads){
        typename std::list<Node*>::iterator it = LocalQueues[tid].begin();
        typename std::list<Node*>::iterator end = LocalQueues[tid].end();

        while(it != end){
            if(!isReferenced(*it)){
                FreeQueues[tid].push_back(*it);

                it = LocalQueues[tid].erase(it);
            }
            else {
                ++it;
            }
        }
        
        Node* free = FreeQueues[tid].front();
        FreeQueues[tid].pop_front();

        return free;
    }
#endif

    //There was no way to get a free node, allocate a new one
    return new Node();
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
bool HazardManager<Node, Threads, Size, Prefill>::isReferenced(Node* node){
#ifdef DEBUG
    for(unsigned int tid = 0; tid < Threads; ++tid){
        for(unsigned int i = 0; i < Size; ++i){
            if(Pointers.at(tid).at(i) == node){
                return true;
            }
        }
    }
#else
    for(unsigned int tid = 0; tid < Threads; ++tid){
        for(unsigned int i = 0; i < Size; ++i){
            if(Pointers[tid][i] == node){
                return true;
            }
        }
    }
#endif

    return false;
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::publish(Node* node, unsigned int i){
#ifdef DEBUG
    Pointers.at(thread_num).at(i) = node;
#else
    Pointers[thread_num][i] = node;
#endif
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::release(unsigned int i){
#ifdef DEBUG
    Pointers.at(thread_num).at(i) = nullptr;
#else
    Pointers[thread_num][i] = nullptr;
#endif
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::releaseAll(){
#ifdef DEBUG
    for(unsigned int i = 0; i < Size; ++i){
        Pointers.at(thread_num).at(i) = nullptr;
    }
#else
    for(unsigned int i = 0; i < Size; ++i){
        Pointers[thread_num][i] = nullptr;
    }
#endif
}

#endif
