Performance comparison of concurrent Binary Search Trees in C++
===============================================================

This project aims to compare several concurrent implementation of Binary Search Tree in C++:

* SkipList
* Non-Blocking Binary Search Trees
* Optimistic AVL Tree
* Lock-Free Multiway Search Tree
* Counter-Based Tree

Note: The code has only been tested on Intel processors. It is possible that there are some differences with other processors. It has only been tested under Linux and with GCC. This application will not build under Windows or under another compiler. 

Build
-----

CMake is used to build the project: 

    cmake .
    make -j9
  
Warning: GCC 4.6 at least is necessary to build this project. 

Launch tests
------------

The tests can be launched easily:

    ./bin/btrees -test
    
Note: The full tests can take about 30 minutes to complete on not-very modern computer and can takes more than 2GB of memory. 

Launch memory benchmark
-----------------------

The memory benchmark is separated in two parts. The first (low) tests the memory consumption with range in [0, size] and the second (high) tests the memory consumption on higher range [0, INT_MAX]:

    ./bin/memory -high
    ./bin/memory -low

Note: The memory benchmark needs at least 6GB of memory to run. 

Launch the benchmark
--------------------

The full benchmark can be run like this: 

    ./bin/btrees -test
    
Note: Even on modern computer, the benchmark can takes more than 10 hours to run and needs several GB of memory. On ancient hardward, it can easily takes about 24 hours to complete. 