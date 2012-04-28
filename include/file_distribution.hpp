#ifndef FILE_DISTRIBUTION
#define FILE_DISTRIBUTION

#include <iostream>
#include <fstream>

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
                std::cout << "Unable to open the file " << file << std::endl;
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
