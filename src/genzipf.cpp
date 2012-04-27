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

int zipf(int n);               // Returns a Zipf random variable
void init_zipf(int n);              
double rand_val();                // Jain's RNG
void init_rand_val(int seed);   

double *powers;
double *inverse_powers;
double *c_powers;

int main(int/* argc*/, char *argv[]){
    FILE   *fp;                   // File pointer to output file
    char   file_name[256];        // Output file name string
    char   temp_string[256];      // Temporary string variable
    double alpha;                 // Alpha parameter
    int	n;                     // N parameter
    int    num_values;            // Number of values
    int    zipf_rv;               // Zipf random variable
    int    i;                     // Loop counter
    int	 *histogram;
    int	 *universe;
    int k,tmp;

    strcpy(file_name, argv[1]);
    fp = fopen(file_name, "w");
    if (fp == NULL){
        printf("ERROR in creating output file (%s) \n", file_name);
        exit(1);
    }

    srand(time(NULL));
    init_rand_val(rand());

    strcpy(temp_string, argv[2]);
    alpha = atof(temp_string);

    strcpy(temp_string, argv[3]);
    n = atoi(temp_string);

    strcpy(temp_string, argv[4]);
    num_values = atoi(temp_string);

    histogram = (int *) malloc(sizeof(int)*n);
    memset(histogram, 0, sizeof(int)*n);

    universe = (int *) malloc(sizeof(int)*n);
    memset(universe, 0, sizeof(int)*n);

    //calculate powers
    powers = (double *) malloc(sizeof(double)*n);
    inverse_powers = (double *) malloc(sizeof(double)*n);
    c_powers = (double *) malloc(sizeof(double)*n);

    for (i = 0; i < n; i++){
        powers[i]=pow((double) i, alpha);
        inverse_powers[i]=1.0/powers[i];
    }

    // Set up initial identity permutation
    for (i = 0; i < n; i++){
        universe[i] = i;
    }

    // Perform the Knuth shuffle
    while (i > 1) {
        i--;
        k = rand()%n;
        tmp = universe[k];
        universe[k] = universe[i];
        universe[i] = tmp;
    }

    init_zipf(n);
    
    // Generate and output zipf random variables
    for (i=0; i<num_values; i++){
        zipf_rv = zipf(n) % n;
        histogram[universe[zipf_rv]]++;
        fprintf(fp, "%d\n", universe[zipf_rv]);
    }

    //Close the output file
    fclose(fp);

    return 0;
}

//Compute normalization constant
void init_zipf(int n){
    double c = 0;
    for (int i=1; i<=n; i++){
        c = c + inverse_powers[i];
    }

    c = 1.0 / c;

    for (int i=1; i<=n; i++){
        c_powers[i]= c / powers[i];
    }
}

int zipf(int n){
    double z;                     // Uniform random number (0 < z < 1)
    double sum_prob;              // Sum of probabilities
    double zipf_value = 0;        // Computed exponential value to be returned

    // Pull a uniform random number (0 < z < 1)
    do {
        z = rand_val();
    } while ((z == 0) || (z == 1));

    // Map z to the value
    sum_prob = 0;
    for (int i=1; i<=n; i++){
        sum_prob = sum_prob + c_powers[i];
        if (sum_prob >= z){
            zipf_value = i;
            break;
        }
    }

    return(zipf_value);
}

static long x = 0;

void init_rand_val(int seed){
    x = seed;
}

double rand_val(){
    const long  a =      16807;  // Multiplier
    const long  m = 2147483647;  // Modulus
    const long  q =     127773;  // m div a
    const long  r =       2836;  // m mod a
    
    long        x_div_q;         // x divided by q
    long        x_mod_q;         // x modulo q
    long        x_new;           // New x value

    // RNG using integer arithmetic
    x_div_q = x / q;
    x_mod_q = x % q;
    x_new = (a * x_mod_q) - (r * x_div_q);
    if (x_new > 0){
        x = x_new;
    } else {
        x = x_new + m;
    }

    // Return a random value between 0.0 and 1.0
    return((double) x / m);
}
