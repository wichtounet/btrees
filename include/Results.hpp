#ifndef RESULTS
#define RESULTS

#include <vector>
#include <map>
#include <string>

typedef std::map<std::string, std::vector<std::vector<unsigned long>>> results_map;
typedef std::map<std::string, std::vector<unsigned long>> stats_map;
typedef std::map<std::string, int> currents_map;

/*!
 * Simple class to store the results of a bench and write them to a file. 
 */
class Results {
    public:
        void start(const std::string& name);
        void set_max(int max);
        
        void add_result(const std::string& structure, unsigned long value); 
        
        void finish();

    private: 
        results_map values;
        stats_map stats;
        currents_map current;
        currents_map level;

        std::string name;
        int max;

        void compute_stats();
};

#endif
