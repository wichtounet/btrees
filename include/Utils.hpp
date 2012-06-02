#ifndef UTILS
#define UTILS

/*!
 * Compare and Swap a pointer. 
 * \param ptr The pointer to swap.
 * \param old The expected value.
 * \param value The new value to set. 
 * \return true if the CAS suceeded, otherwise false. 
 */
template<typename T>
bool inline CASPTR(T** ptr, T* old, T* value){
    return __sync_bool_compare_and_swap(ptr, old, value);
}

#endif
