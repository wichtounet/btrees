#ifndef AVL_TREE_TREE
#define AVL_TREE_TREE

#include <cmath>

template<int N>
struct zipf_distribution {
    std::mt19937_64 engine; 
    std::uniform_real_distribution<double> distribution;
    
    double skew;
    double c;    

    zipf_distribution(double skew) : engine(time(NULL)), distribution(
                0.0 + std::numeric_limits<double>::epsilon(), 
                1.0 - std::numeric_limits<double>::epsilon()), skew(skew) {
        c = 0;

        for (int i = 1; i <= N; i++){
            c = c + (1.0 / pow((double) i, skew));
        }

        c = 1.0 / c;
    }

    int next(){
        double z = distribution(engine);

        double sum_prob = 0.0;              

        for (int i = 1; i <= N; i++){
            sum_prob += c / pow((double) i, skew);
            if (sum_prob >= z){
                return i;
            }
        }

        return 0;

        assert(false);
    }
};

#endif
