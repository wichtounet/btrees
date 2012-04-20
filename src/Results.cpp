#include <iostream>
#include <fstream>

#include "Results.hpp"

void Results::add_result(const std::string& structure, unsigned long value){
    values[structure].push_back(value);
}

void Results::start(const std::string& name){
    values.clear();
    this->name = name;
}

void Results::finish(){
    results_map::iterator it = values.begin();
    results_map::iterator end = values.end();

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
