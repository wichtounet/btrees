#include <cstdlib>
#include <ctime>

int randomLevel(int max){
    srand(time(NULL));

    return rand() % max;
}
