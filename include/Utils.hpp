#ifndef UTILS
#define UTILS

#include <cstdlib>
#include <ctime>
#include <climits>
        
template<typename T>
bool inline CASPTR(T** ptr, T* old, T* value){
    return __sync_bool_compare_and_swap(ptr, old, value);
}

#endif
