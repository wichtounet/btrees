#ifndef HAZARD_MANAGER
#define HAZARD_MANAGER

template<typename Node, int Threads, int Size = 2, int Prefill = 50>
class HazardManager {
    public:
        HazardManager();
        ~HazardManager();

        /* Manage nodes */
        void releaseNode(Node* node);
        Node* getFreeNode();

        /* Manage references  */
        void publish(Node* node, int i);
        void release(int i);

    private:
        Node* Pointers[Threads][Size];
        Node* LocalQueues[Threads][2];
        Node* FreeQueues[Threads][2];
        int CountFreeNodes[Threads];
        
        bool isReferenced(Node* node);
};

template<typename Node, int Threads, int Size, int Prefill>
HazardManager<Node, Threads, Size, Prefill>::HazardManager(){
    for(int tid = 0; tid < Threads; ++tid){
        for(int j = 0; j < Size; ++j){
            Pointers[tid][j] = nullptr;
        }

        LocalQueues[tid][0] = LocalQueues[tid][1] = nullptr;
        CountFreeNodes[tid] = 0;

        if(Prefill > 0){
            FreeQueues[tid][0] = FreeQueues[tid][1] = new Node();
            
            for(int i = 1; i < Prefill; i++){
                Node* node = new Node();

                FreeQueues[tid][1]->nextNode = node;
                FreeQueues[tid][1] = node;
            }
        } else {
            FreeQueues[tid][0] = FreeQueues[tid][1] = nullptr;
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

template<typename Node, int Threads, int Size, int Prefill>
HazardManager<Node, Threads, Size, Prefill>::~HazardManager(){
    for(int tid = 0; tid < Threads; ++tid){
        //No need to delete Hazard Pointers because each thread need to release its published references

        //Delete all the elements of both queues
        deleteAll(LocalQueues[tid]);
        deleteAll(FreeQueues[tid]);
    }
}

template<typename Node, int Threads, int Size, int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::releaseNode(Node* node){
    int tid = thread_num;

    node->nextNode = nullptr;

    //Add the node to the localqueue
    if(!LocalQueues[tid][0]){
        LocalQueues[tid][0] = LocalQueues[tid][1] = node;
    } else {
        LocalQueues[tid][1]->nextNode = node;
        LocalQueues[tid][1] = node;
    }

    //There is one more available node in the local queue
    ++CountFreeNodes[tid];
}

template<typename Node, int Threads, int Size, int Prefill>
Node* HazardManager<Node, Threads, Size, Prefill>::getFreeNode(){
    int tid = thread_num;

    //First test if there are free nodes for this thread
    Node* free = FreeQueues[tid][0];
    if(free){
        FreeQueues[tid][0] = free->nextNode;

        return free;
    }

    //If there are enough local nodes, move then to the free queue
    if(CountFreeNodes[tid] > (Size + 1) * Threads){
        Node* node; 
        Node* pred = LocalQueues[tid][0];

        //Little optimization for the first insertion to the free queue (avoid a branch in the second loop)
        if(!isReferenced(pred)){
            FreeQueues[tid][0] = FreeQueues[tid][1] = pred;
                
            //Pop the node from the local queue
            LocalQueues[tid][0] = pred->nextNode;
            pred->nextNode = nullptr;
            --CountFreeNodes[tid];
        } else {
            while((node = pred->nextNode)){
                if(!isReferenced(node)){
                    if(!(pred->nextNode = node->nextNode)){
                        LocalQueues[tid][1] = pred;
                    }
            
                    FreeQueues[tid][0] = FreeQueues[tid][1] = node;
                    node->nextNode = nullptr;
                
                    --CountFreeNodes[tid];

                    break;
                } else {
                    pred = node;
                }
            }
        }

        //Enqueue all the other free nodes to the free queue
        while((node = pred->nextNode)){
            if(!isReferenced(node)){
                if(!(pred->nextNode = node->nextNode)){
                    LocalQueues[tid][1] = pred;
                }

                FreeQueues[tid][1]->nextNode = node;
                FreeQueues[tid][1] = node;
                node->nextNode = nullptr;

                --CountFreeNodes[tid];
            } 
            
            pred = node;
        }
    
        node = FreeQueues[tid][0];

        //The algorithm ensures that there will be at least one node in the free queue
        assert(node);
        
        FreeQueues[tid][0] = node->nextNode;

        return node;
    }

    //There was no way to get a free node, allocate a new one
    return new Node();
}

template<typename Node, int Threads, int Size, int Prefill>
bool HazardManager<Node, Threads, Size, Prefill>::isReferenced(Node* node){
    for(int tid = 0; tid < Threads; ++tid){
        for(int i = 0; i < Size; ++i){
            if(Pointers[tid][i] == node){
                return true;
            }
        }
    }

    return false;
}
        

template<typename Node, int Threads, int Size, int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::publish(Node* node, int i){
    Pointers[thread_num][i] = node;
}

template<typename Node, int Threads, int Size, int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::release(int i){
    Pointers[thread_num][i] = nullptr;
}

#endif
