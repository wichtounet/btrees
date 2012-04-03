#ifndef ZIPF
#define ZIPF

#include <cassert>
#include <cmath>
#include <limits>

template<class IntType = int, class RealType = double>
class discrete_distribution {
    public:
        typedef RealType input_type;
        typedef IntType result_type;

    private:
        std::vector<input_type> probabilities_;
        std::vector<result_type> aliases_;

        template<class RandomAccessIterator>
        void make_table(RandomAccessIterator first, RandomAccessIterator last,
                std::random_access_iterator_tag)
        {
            result_type n = std::distance(first, last);

            while (first != last) {
                probabilities_.push_back(*first - input_type(1.0/n));
                ++first;
            }

            aliases_.resize(n, n);

            for (result_type i = 0; i < n; ++i) {
                result_type j = 0;
                while (j < n && aliases_[j] < n) ++j;

                result_type min_i(j);
                result_type max_i(j);
                input_type min_v(probabilities_[min_i]);
                input_type max_v(probabilities_[max_i]);

                for (++j; j < n; ++j) {
                    if (aliases_[j] < n) continue;
                    if (probabilities_[j] < min_v) {
                        min_i = j;
                        min_v = probabilities_[j];
                    }
                    else if (probabilities_[j] > max_v) {
                        max_i = j;
                        max_v = probabilities_[j];
                    }
                }

                if (min_i != max_i) {
                    aliases_[min_i] = max_i;
                    probabilities_[min_i] = input_type(1.0) + min_v*n;
                    probabilities_[max_i] = min_v + max_v;
                }
                else {
                    probabilities_[min_i] = 1.0;
                }
            }
        }

        template<class InputIterator>
        void make_normalized_table(InputIterator first, InputIterator last){
            std::vector<input_type> dist;
            input_type sum = *first;
            dist.push_back(*first);
            while (++first != last) {
                sum += *first;
                dist.push_back(*first);
            }

            for (typename std::vector<input_type>::iterator i = dist.begin();
                    i != dist.end(); ++i) {
                *i /= sum;
            }

            make_table(dist.begin(), dist.end(), std::random_access_iterator_tag());
        }

    public:
        template<class InputIterator>
        discrete_distribution(InputIterator first, InputIterator last) : probabilities_(), aliases_(){
            make_normalized_table(first, last);
        }

        result_type num() const { 
            return result_type(probabilities_.size());
        }

        template<class Engine>
        result_type operator()(Engine& eng){
            std::uniform_real_distribution<input_type> ud;
            input_type u = ud(eng)*num();
            result_type x = result_type(u);
            u -= x;
            if (u < probabilities_[x]){
                return x;
            }
            
            return aliases_[x];
        }
};

template<class IntType = int, class RealType = double>
class zipf_distribution {
    public:
        typedef RealType input_type;
        typedef IntType result_type;

    private:
        result_type num_;
        input_type shift_;
        input_type exp_;

        typedef discrete_distribution<IntType, RealType> dist_type;
        dist_type dist_;

        dist_type make_dist(result_type num, input_type shift, input_type exp){
            std::vector<input_type> buffer(num + 1, input_type(0));
            
            for (result_type k = 1; k < num; ++k){
                buffer[k] = std::pow((input_type) k + shift, -exp);
            }

            return dist_type(buffer.begin(), buffer.end());
        }

    public:
        zipf_distribution(result_type num, input_type shift, input_type exp)
            : num_(num), shift_(shift), exp_(exp),
            dist_(make_dist(num, shift, exp)){}

        template<class Engine>
        result_type operator()(Engine& eng) { return dist_(eng); }
};

#endif
