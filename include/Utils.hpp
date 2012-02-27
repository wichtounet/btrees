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
