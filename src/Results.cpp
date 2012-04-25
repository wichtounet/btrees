#include <iostream>
#include <fstream>

#include "Results.hpp"

void Results::start(const std::string& name){
    values.clear();
    this->name = name;
    this->max = -1;
}
        
void Results::set_max(int max){
    this->max = max;
}

void Results::add_result(const std::string& structure, unsigned long value){
    if(current[structure] == max){
        ++level[structure];
        current[structure] = 0;
    }

    values[structure][level[structure]].push_back(value);
    
    ++current[structure];
}
        
void Results::compute_stats(){
    auto it = values.begin();
    auto end = values.end();

    while(it != end){
        auto impl = it->first;
        auto data = it->second;

        for(unsigned int i = 0; i < data.size(); ++i){
            unsigned long sum = 0;

            for(auto j : data[i]){
                sum += j;
            }

            stats[impl].push_back(sum / data.size());
        }
    }
}

void Results::finish(){
    compute_stats();

    auto it = stats.begin();
    auto end = stats.end();

    while(it != end){
        std::string file = "graphs/" + it->first + "-" + name + ".dat";

        std::ofstream stream;
        stream.open(file.c_str());

        if(!stream){
            throw "Unable to open the file " + file;
        }

        for(unsigned int i = 0; i < it->second.size(); ++i){
            stream << (i+1) << " " << it->second[i] << std::endl;
        }

        stream.close();

        ++it;
    }
}
