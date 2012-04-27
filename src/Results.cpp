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

    if(values.find(structure) == values.end()){
       values[structure].resize(max); 
    }

    values[structure][current[structure]].push_back(value);
    
    ++current[structure];
}
        
void Results::compute_stats(){
    auto it = values.begin();
    auto end = values.end();

    while(it != end){
        auto impl = it->first;
        auto data = it->second;

        for(unsigned int i = 0; i < data.size(); ++i){
            //If it's not the case, max has been configured too high
            if(data[i].size() > 0){
                unsigned long sum = 0;

                for(auto j : data[i]){
                    sum += j;
                }

                stats[impl].push_back(sum / data[i].size());
            }
        }

        ++it;
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
