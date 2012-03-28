#ifndef UTILS
#define UTILS

#include <cstdlib>
#include <ctime>
#include <climits>
#include <unistd.h>
#include <ios>
#include <iostream>
#include <fstream>
#include <string>
        
template<typename T>
bool inline CASPTR(T** ptr, T* old, T* value){
    return __sync_bool_compare_and_swap(ptr, old, value);
}

void process_mem_usage(double& vm_usage, double& resident_set){
   using std::ios_base;
   using std::ifstream;
   using std::string;

   vm_usage     = 0.0;
   resident_set = 0.0;
   
   ifstream stat_stream("/proc/self/statm",ios_base::in);

   unsigned long vsize;
   unsigned long rss;

   stat_stream >> vsize >> rss; // don't care about the rest

   stat_stream.close();

   long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
   vm_usage     = vsize / 1024.0;
   resident_set = rss * page_size_kb;
}

#endif
