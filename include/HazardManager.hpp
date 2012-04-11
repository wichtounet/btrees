#ifndef HAZARD_MANAGER
#define HAZARD_MANAGER

#include <cassert>

//Thread local id
//Note: __thread is GCC specific
extern __thread unsigned int thread_num;

template<typename Node, unsigned int Threads, unsigned int Size = 2, unsigned int Prefill = 50>
class HazardManager {
    public:
        HazardManager();
        ~HazardManager();

        /* Manage nodes */
        void releaseNode(Node* node);
        Node* getFreeNode();

        /* Manage references  */
        void publish(Node* node, unsigned int i);
        void release(unsigned int i);
        void releaseAll();

    private:
        Node* Pointers[Threads][Size];
        Node* LocalQueues[Threads][2];
        Node* FreeQueues[Threads][2];
        unsigned int CountFreeNodes[Threads];
        
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

        LocalQueues[tid][0] = LocalQueues[tid][1] = nullptr;
        CountFreeNodes[tid] = 0;
        
        FreeQueues[tid][0] = FreeQueues[tid][1] = nullptr;

        if(Prefill > 0){
            for(unsigned int i = 0; i < Prefill; i++){
                Enqueue(FreeQueues, new Node(), tid);
            }
        } 
    }
}

template<typename Node>
inline void deleteAll(Node** Queue){
    //Delete all the elements of the queue
    if(Queue[0]){
        //There are only a single element in the queue
        if(Queue[0] == Queue[1]){
            delete Queue[0];
        }
        //Delete all the elements of the queue            
        else {
            Node* node; 
            Node* pred = Queue[0];

            while((node = pred->nextNode)){
                delete pred;
                pred = node;
            }

            delete Queue[1];
        }
    }
}

static int alloc = 0;
static int releaseCount = 0;

#include <iostream>

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
HazardManager<Node, Threads, Size, Prefill>::~HazardManager(){
    for(unsigned int tid = 0; tid < Threads; ++tid){
        //No need to delete Hazard Pointers because each thread need to release its published references

        //Delete all the elements of both queues
        deleteAll(LocalQueues[tid]);
        deleteAll(FreeQueues[tid]);
    }


    std::cout << "get free node" << alloc << std::endl;
    std::cout << "release nodes" << releaseCount << std::endl;
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::Enqueue(Node* Queue[Threads][2], Node* node, unsigned int tid){
    if(!Queue[tid][0]){
        Queue[tid][0] = Queue[tid][1] = node;
    } else {
        Queue[tid][1]->nextNode = node;
        Queue[tid][1] = node;
    }
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::releaseNode(Node* node){
    ++releaseCount;
    //Add the node to the localqueue
    node->nextNode = nullptr;
    Enqueue(LocalQueues, node, thread_num);

    //There is one more available node in the local queue
    ++CountFreeNodes[thread_num];
}

template<typename Node, unsigned int Threads, unsigned int Size, unsigned int Prefill>
Node* HazardManager<Node, Threads, Size, Prefill>::getFreeNode(){
    ++alloc;
    int tid = thread_num;

    //First test if there are free nodes for this thread
    Node* free = FreeQueues[tid][0];
    if(free){
        FreeQueues[tid][0] = free->nextNode;

        return free;
    }

    //If there are enough local nodes, move then to the free queue
    if(CountFreeNodes[tid] > (Size + 1) * Threads){
        Node* node = nullptr; 
        Node* pred = LocalQueues[tid][0];

        //Little optimization for the first insertion to the free queue (avoid a branch in the second loop)
        if(!isReferenced(pred)){
            Enqueue(FreeQueues, pred, tid);

            //Pop the node from the local queue
            LocalQueues[tid][0] = pred->nextNode;
            --CountFreeNodes[tid];
        } 

        //Enqueue all the other free nodes to the free queue
        while((node = pred->nextNode)){
            if(!isReferenced(node)){
                if(!(pred->nextNode = node->nextNode)){
                    LocalQueues[tid][1] = pred;
                }

                Enqueue(FreeQueues, pred, tid);

                --CountFreeNodes[tid];
            }
            
            pred = node;
        }

        FreeQueues[tid][1]->nextNode = nullptr;
    
        node = FreeQueues[tid][0];

        //The algorithm ensures that there will be at least one node in the free queue
        assert(node);
        
        //Pop it out of the queue
        FreeQueues[tid][0] = node->nextNode;

        return node;
    }

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
