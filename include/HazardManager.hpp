#ifndef HAZARD_MANAGER
#define HAZARD_MANAGER

#include <cassert>

//Thread local id
//Note: __thread is GCC specific
extern __thread unsigned int thread_num;

#define CountFree() std::cout << "Free contains " << FreeQueues[tid].size() << std::endl;
#define CountLocal() std::cout << "Local contains " << LocalQueues[tid].size() << std::endl;

#include <iostream>
#include <list>
#include <vector>

template<typename Node, unsigned int Threads, unsigned int Size = 2, unsigned int Prefill = 50>
class HazardManager {
    public:
        HazardManager();
     //   ~HazardManager();

        HazardManager(const HazardManager& rhs) = delete;

        /* Manage nodes */
        void releaseNode(Node* node);
        Node* getFreeNode();

        /* Manage references  */
        void publish(Node* node, unsigned int i);
        void release(unsigned int i);
        void releaseAll();

    private:
        Node* Pointers[Threads][Size];

        std::list<Node*> LocalQueues[Threads];
        std::list<Node*> FreeQueues[Threads];
        
        bool isReferenced(Node* node);

        void Enqueue(Node* Queue[Threads][2], Node* node, unsigned int tid);

        /* Verify the template parameters */
        static_assert(Threads > 0, "The number of threads must be greater than 0");
        static_assert(Size > 0, "The number of hazard pointers must greater than 0");
};

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
HazardManager<Node, Threads, Size, Prefill>::HazardManager(){
    for(unsigned int tid = 0; tid < Threads; ++tid){
        for(unsigned int j = 0; j < Size; ++j){
            Pointers[tid][j] = nullptr;
        }

        if(Prefill > 0){
            for(unsigned int i = 0; i < Prefill; i++){
                FreeQueues[tid].push_back(new Node());
            }
        } 

        CountFree();
        CountLocal();
    }
}

static int alloc = 0;
static int releaseCount = 0;
static int freeCount = 0;

template<typename Node>
inline void deleteAll(Node** Queue){
    //Delete all the elements of the queue
    if(Queue[0]){
        //There are only a single element in the queue
        if(Queue[0] == Queue[1]){
            delete Queue[0];
            ++freeCount;
        }
        //Delete all the elements of the queue            
        else {
            Node* node; 
            Node* pred = Queue[0];

            while((node = pred->nextNode)){
                delete pred;
                pred = node;
                ++freeCount;
            }

            delete Queue[1];
            ++freeCount;
        }
    }
}

/*template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
HazardManager<Node, Threads, Size, Prefill>::~HazardManager(){
    std::cout << "Destroy" << std::endl;

    for(unsigned int tid = 0; tid < Threads; ++tid){
        //No need to delete Hazard Pointers because each thread need to release its published references

        CountFree();
        CountLocal();

        /*while(!LocalQueues[tid].empty()){
            delete LocalQueues[tid].front();
            LocalQueues[tid].pop_front();
            ++freeCount;
        }
        
        while(!FreeQueues[tid].empty()){
            delete FreeQueues[tid].front();
            FreeQueues[tid].pop_front();
            ++freeCount;
        }*/
    /*}

    std::cout << "get free node " << alloc << std::endl;
    std::cout << "release nodes " << releaseCount << std::endl;
    std::cout << "freed nodes " << freeCount << std::endl;
}*/

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::Enqueue(Node* Queue[Threads][2], Node* node, unsigned int tid){
    if(!Queue[tid][0]){
        Queue[tid][0] = Queue[tid][1] = node;
        node->nextNode = nullptr;
    } else {
        Queue[tid][1]->nextNode = node;
        Queue[tid][1] = node;
    }
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::releaseNode(Node* node){
    if(!node){
        assert(false);
    }

    std::cout << "Release node" << node << std::endl;
    unsigned int tid = thread_num;
    ++releaseCount;
    //Add the node to the localqueue
    CountLocal();
    LocalQueues[tid].push_back(node);
    CountLocal();
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
Node* HazardManager<Node, Threads, Size, Prefill>::getFreeNode(){
    std::cout << "Get Free node" << std::endl;

    ++alloc;
    int tid = thread_num;

    if(!FreeQueues[tid].empty()){
        std::cout << "Get from free queue" << std::endl;
        CountFree();
        CountLocal();
        Node* free = FreeQueues[tid].front();
        FreeQueues[tid].pop_front();
        CountFree();
        CountLocal();

        return free;
    }

    //If there are enough local nodes, move then to the free queue
    if(LocalQueues[tid].size() > (Size + 1) * Threads){
        std::cout << "Before" << std::endl;
        CountFree();
        CountLocal();
        
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
        
        std::cout << "After" << std::endl;
        CountFree();
        CountLocal();
        
        Node* free = FreeQueues[tid].front();
        FreeQueues[tid].pop_front();

        std::cout << "After 2" << std::endl;
        CountFree();
        CountLocal();

        return free;
    }
    
    std::cout << "Before malloc" << std::endl;
    CountFree();
    CountLocal();

    //There was no way to get a free node, allocate a new one
    return new Node();
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
bool HazardManager<Node, Threads, Size, Prefill>::isReferenced(Node* node){
    for(unsigned int tid = 0; tid < Threads; ++tid){
        for(unsigned int i = 0; i < Size; ++i){
            if(Pointers[tid][i] == node){
                return true;
            }
        }
    }

    return false;
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::publish(Node* node, unsigned int i){
    Pointers[thread_num][i] = node;
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::release(unsigned int i){
    Pointers[thread_num][i] = nullptr;
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::releaseAll(){
    for(unsigned int i = 0; i < Size; ++i){
        Pointers[thread_num][i] = nullptr;
    }
}

#endif
