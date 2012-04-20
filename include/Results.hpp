#ifndef RESULTS
#define RESULTS

#include <vector>
#include <map>
#include <string>

typedef std::map<std::string, std::vector<unsigned long>> results_map;

class Results {
    public:
        void add_result(const std::string& structure, unsigned long value); 

        void start(const std::string& name);
        void finish();

    private: 
        results_map values;

        std::string name;
};

#endif
