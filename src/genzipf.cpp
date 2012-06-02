//==================================================== file = genzipf.c =====
//=  Program to generate Zipf (power law) distributed random variables      =
//===========================================================================
//=  Notes: 1) Writes to a user specified output file                       =
//=         2) Generates user specified number of values                    =
//=         3) Run times is same as an empirical distribution generator     =
//=         4) Implements p(i) = C/i^alpha for i = 1 to N where C is the    =
//=            normalization constant (i.e., sum of p(i) = 1).              =
//=-------------------------------------------------------------------------=
//= Example user input:                                                     =
//=                                                                         =
//=   ---------------------------------------- genzipf.c -----              =
//=   -     Program to generate Zipf random variables        -              =
//=   --------------------------------------------------------              =
//=   Output file name ===================================> output.dat      =
//=   Random number seed =================================> 1               =
//=   Alpha vlaue ========================================> 1.0             =
//=   N value ============================================> 1000            =
//=   Number of values to generate =======================> 5               =
//=   --------------------------------------------------------              =
//=   -  Generating samples to file                          -              =
//=   --------------------------------------------------------              =
//=   --------------------------------------------------------              =
//=   -  Done!                                                              =
//=   --------------------------------------------------------              =
//=-------------------------------------------------------------------------=
//= Example output file ("output.dat" for above):                           =
//=                                                                         =
//=   1                                                                     =
//=   1                                                                     =
//=   161                                                                   =
//=   17                                                                    =
//=   30                                                                    =
//=-------------------------------------------------------------------------=
//=  Build: bcc32 genzipf.c                                                 =
//=-------------------------------------------------------------------------=
//=  Execute: genzipf                                                       =
//=-------------------------------------------------------------------------=
//=  Author: Kenneth J. Christensen                                         =
//=          University of South Florida                                    =
//=          WWW: http://www.csee.usf.edu/~christen                         =
//=          Email: christen@csee.usf.edu                                   =
//=-------------------------------------------------------------------------=
//=  History: KJC (11/16/03) - Genesis (from genexp.c)                      =
//===========================================================================

#include <assert.h>             // Needed for assert() macro
#include <stdio.h>              // Needed for printf()
#include <stdlib.h>             // Needed for exit() and ato*()
#include <time.h>
#include <math.h>               // Needed for pow()
#include <string.h>

#include <iostream>
#include <algorithm>

int zipf(int n);               // Returns a Zipf random variable
void init_zipf(int n);              
double rand_val();                // Jain's RNG
void init_rand_val(int seed);   

double *powers;
double *inverse_powers;
double *c_powers;

void slow_generate(const char* file, double alpha, int n, int num_value);

int main(int /*argc*/, char*[] /*argv*/){
    //Generate the histograms
    slow_generate("zipf/zipf-histo-02", 0.2, 1000, 50000);
    slow_generate("zipf/zipf-histo-08", 0.8, 1000, 50000);
    slow_generate("zipf/zipf-histo-12", 1.2, 1000, 50000);
    slow_generate("zipf/zipf-histo-18", 1.8, 1000, 50000);

    //Generate the data for the benchmarks
    slow_generate("zipf/zipf-00-2000", 0.0, 2000, 1000000);
    slow_generate("zipf/zipf-02-2000", 0.2, 2000, 1000000);
    slow_generate("zipf/zipf-04-2000", 0.4, 2000, 1000000);
    slow_generate("zipf/zipf-06-2000", 0.6, 2000, 1000000);
    slow_generate("zipf/zipf-08-2000", 0.8, 2000, 1000000);
    slow_generate("zipf/zipf-10-2000", 1.0, 2000, 1000000);
    slow_generate("zipf/zipf-12-2000", 1.2, 2000, 1000000);
    slow_generate("zipf/zipf-14-2000", 1.4, 2000, 1000000);
    slow_generate("zipf/zipf-16-2000", 1.6, 2000, 1000000);
    slow_generate("zipf/zipf-18-2000", 1.8, 2000, 1000000);
    slow_generate("zipf/zipf-20-2000", 2.0, 2000, 1000000);

    slow_generate("zipf/zipf-00-20000", 0.0, 20000, 1000000);
    slow_generate("zipf/zipf-02-20000", 0.2, 20000, 1000000);
    slow_generate("zipf/zipf-04-20000", 0.4, 20000, 1000000);
    slow_generate("zipf/zipf-06-20000", 0.6, 20000, 1000000);
    slow_generate("zipf/zipf-08-20000", 0.8, 20000, 1000000);
    slow_generate("zipf/zipf-10-20000", 1.0, 20000, 1000000);
    slow_generate("zipf/zipf-12-20000", 1.2, 20000, 1000000);
    slow_generate("zipf/zipf-14-20000", 1.4, 20000, 1000000);
    slow_generate("zipf/zipf-16-20000", 1.6, 20000, 1000000);
    slow_generate("zipf/zipf-18-20000", 1.8, 20000, 1000000);
    slow_generate("zipf/zipf-20-20000", 2.0, 20000, 1000000);

    slow_generate("zipf/zipf-00-200000", 0.0, 200000, 1000000);
    slow_generate("zipf/zipf-02-200000", 0.2, 200000, 1000000);
    slow_generate("zipf/zipf-04-200000", 0.4, 200000, 1000000);
    slow_generate("zipf/zipf-06-200000", 0.6, 200000, 1000000);
    slow_generate("zipf/zipf-08-200000", 0.8, 200000, 1000000);
    slow_generate("zipf/zipf-10-200000", 1.0, 200000, 1000000);
    slow_generate("zipf/zipf-12-200000", 1.2, 200000, 1000000);
    slow_generate("zipf/zipf-14-200000", 1.4, 200000, 1000000);
    slow_generate("zipf/zipf-16-200000", 1.6, 200000, 1000000);
    slow_generate("zipf/zipf-18-200000", 1.8, 200000, 1000000);
    slow_generate("zipf/zipf-20-200000", 2.0, 200000, 1000000);

    slow_generate("zipf/zipf-00-2000000", 0.0, 2000000, 1000000);
    slow_generate("zipf/zipf-02-2000000", 0.2, 2000000, 1000000);
    slow_generate("zipf/zipf-04-2000000", 0.4, 2000000, 1000000);
    slow_generate("zipf/zipf-06-2000000", 0.6, 2000000, 1000000);
    slow_generate("zipf/zipf-08-2000000", 0.8, 2000000, 1000000);
    slow_generate("zipf/zipf-10-2000000", 1.0, 2000000, 1000000);
    slow_generate("zipf/zipf-12-2000000", 1.2, 2000000, 1000000);
    slow_generate("zipf/zipf-14-2000000", 1.4, 2000000, 1000000);
    slow_generate("zipf/zipf-16-2000000", 1.6, 2000000, 1000000);
    slow_generate("zipf/zipf-18-2000000", 1.8, 2000000, 1000000);
    slow_generate("zipf/zipf-20-2000000", 2.0, 2000000, 1000000);

    return 0;
}

int      slow_zipf(double alpha, int n);  // Returns a Zipf random variable
double   slow_rand_val();         // Jain's RNG
void     slow_init(double alpha, int n);

static int seed;
static double c;

void slow_generate(const char* file_name, double alpha, int n, int num_values){
    std::cout << "Slow Generate " << num_values << " values in [0, " << n << "] with a skew of " << alpha << std::endl; 

    srand(time(NULL));
    seed = rand();

    slow_init(alpha, n);
    
    FILE   *fp;                   // File pointer to output file
    fp = fopen(file_name, "w");
    if (fp == NULL){
        printf("ERROR in creating output file (%s) \n", file_name);
        exit(1);
    }

    // Generate and output zipf random variables
    for (int i=0; i<num_values; i++){
        int zipf_rv = slow_zipf(alpha, n);
        fprintf(fp, "%d \n", zipf_rv);
    }

    fclose(fp);
}

void slow_init(double alpha, int n){
    for (int i=1; i<=n; i++)
        c = c + (1.0 / pow((double) i, alpha));
    c = 1.0 / c;
}

int slow_zipf(double alpha, int n){
    double z;                     // Uniform random number (0 < z < 1)
    double zipf_value = 0.0;      // Computed exponential value to be returned

    // Pull a uniform random number (0 < z < 1)
    do {
        z = slow_rand_val();
    } while ((z == 0) || (z == 1));

    // Map z to the value
    double sum_prob = 0;

    for (int i=1; i<=n; i++){
        sum_prob = sum_prob + c / pow((double) i, alpha);

        if (sum_prob >= z){
            zipf_value = i;
            break;
        }
    }

    return zipf_value;
}

double slow_rand_val(){
    const long  a =      16807;  // Multiplier
    const long  m = 2147483647;  // Modulus
    const long  q =     127773;  // m div a
    const long  r =       2836;  // m mod a
    long        x_div_q;         // x divided by q
    long        x_mod_q;         // x modulo q
    long        x_new;           // New x value

    // RNG using integer arithmetic
    x_div_q = seed / q;
    x_mod_q = seed % q;
    x_new = (a * x_mod_q) - (r * x_div_q);

    if (x_new > 0)
        seed = x_new;
    else
        seed = x_new + m;

    // Return a random value between 0.0 and 1.0
    return((double) seed / m);
}
