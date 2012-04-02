#ifndef ZIPF
#define ZIPF

#include <cassert>
#include <cmath>
#include <limits>

struct zipf_distribution {
    std::mt19937_64 engine; 
    std::uniform_real_distribution<double> distribution;
    
    const int N;
    std::vector<double> probs;

    zipf_distribution(int N, double skew) : engine(time(NULL)), distribution(
                0.0 + std::numeric_limits<double>::epsilon(), 
                1.0 - std::numeric_limits<double>::epsilon()), N(N){
        double c = 0;

        std::vector<double> pows;
        pows.reserve(N);
        pows.push_back(0);

        for (int i = 1; i <= N; i++){
            pows.push_back(pow((double) i, skew));
            c += (1.0 / pows[i]);
        }

        c = 1.0 / c;
        
        probs.reserve(N);
        probs.push_back(0);

        double sum_prob = 0.0;
        
        for (int i = 1; i <= N; i++){
            sum_prob += c / pows[i];
            probs.push_back(sum_prob);
        }
    }

    int next(){
        double z = distribution(engine);

        for (int i = 1; i <= N; i++){
            if (probs[i] >= z){
                return i;
            }
        }

        return 0;
    }
};

struct fast_zipf_distribution {
    std::vector<int> rands;
    int i = 0;
    int max = 0;

    fast_zipf_distribution(int N, double skew) : max(N * 100) {
        rands.reserve(max);
    
        zipf_distribution distribution(N, skew);

        for(int i = 0; i < max; ++i){
            rands.push_back(distribution.next());
        }
    }

    int next(){
        return rands[i++ % max];
    }
};

#endif
