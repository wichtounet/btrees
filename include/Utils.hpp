#ifndef UTILS
#define UTILS

#include <cstdlib>
#include <ctime>
#include <climits>

int random(int max){
    srand(time(NULL));

    return rand() % max;
}

int random(double p, int maxLevel){
    int level = 0;

    while (((double) rand() / (double) RAND_MAX) < p){
        level++;
    }

    return (level >= maxLevel) ? maxLevel : level;
}
        
template<typename T>
bool inline CASPTR(T** ptr, T* old, T* value){
    return __sync_bool_compare_and_swap(ptr, old, value);
}

template<typename T>
union CONVERSION {
    T* node;
    unsigned long value;
};

template<typename T>
bool isMarked(T* node){
    CONVERSION<T> conv;
    conv.node = node;

    return conv.value & 0x1;
}

template<typename T>
void Set(T** ptr, T* ref, bool value){
    CONVERSION<T> current;
    current.node = ref;
    
    if(value){
        current.value |= 1l; 
    } else {
        current.value &= (~0l - 1);
    }

    *ptr = current.node;
}

template<typename T>
bool CompareAndSet(T** ptr, T* ref, T* newRef, bool value, bool newValue){
    CONVERSION<T> current;
    current.node = ref;

    if(value){
        current.value |= 1l; 
    } else {
        current.value &= (~0l - 1);
    }

    CONVERSION<T> next;
    next.node = newRef;
    
    if(newValue){
        next.value |= 1l; 
    } else {
        next.value &= (~0l - 1);
    }
   
    return CASPTR(ptr, current.node, next.node); 
}

#endif
