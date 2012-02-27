#include <cstdlib>
#include <ctime>

int randomLevel(int max){
    srand(time(NULL));

    return rand() % max;
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
