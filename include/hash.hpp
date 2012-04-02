#ifndef HASH
#define HASH

template<typename T>
int hash(T value);

template<>
inline int hash<int>(int value){
    return value;
}

#endif
