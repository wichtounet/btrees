#ifndef FILE_DISTRIBUTION
#define FILE_DISTRIBUTION

#include <iostream>
#include <fstream>

/*!
 * \brief A sample random distribution that takes its value in a file. 
 * All the values of the file will be loaded in memory at construction time of the distribution. 
 * \param Type The type of value contained in the file. 
 */
template<class Type = int>
class file_distribution {
    public:
        typedef Type result_type;
        
    private:
        std::vector<Type> values;

        std::uniform_int_distribution<int> distribution;

    public:
        file_distribution(const std::string& file, unsigned int size){
            std::ifstream stream(file.c_str());

            if(!stream){
                throw "Unable to open the file " + file;
            }

            for(unsigned int i = 0; i < size; ++i){
                int value = 0;
                stream >> value;
                values.push_back(value);
            }

            distribution = std::uniform_int_distribution<int>(0, size - 1);
        }
        
        result_type operator()(unsigned int i){
            return values[i % values.size()];
        }
        
        template<class Engine>
        result_type operator()(Engine& eng){
            return values[distribution(eng)];
        }
};

#endif
