#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/libccompat.h>
#include <nautilus/random.h>

#define ERROR(fmt, args...) ERROR_PRINT("omptest: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("omptest: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("omptest: " fmt, ##args)

static inline uint16_t random()
{
    uint16_t t;
    nk_get_rand_bytes((uint8_t *)&t,sizeof(t));
    return t;
}

#define MAXN 5100  /* Max value of N */

static int N;  /* Matrix size */
static int procs;  /* Number of processors to use */

/* Matrices and vectors */
static volatile float A[MAXN][MAXN], B[MAXN], X[MAXN];
static volatile float ORA[MAXN][MAXN], ORB[MAXN], ORX[MAXN];
/* A * X = B, solve for X */

static int seed;
/* Prototype */
static void gauss();  /* The function you will provide.
		* It is this routine that is timed.
		* It is called only on the parent.
		*/

/* Initialize A and B (and X to 0.0s) */
static void initialize_inputs() {
  int row, col;
 
  printf("\nInitializing...\n");
  //   #pragma omp parallel num_threads(8)
  {
  //  #pragma omp for private(row,col) schedule(static,1) nowait
    for (col = 0; col < N; col++) {
   
    for (row = 0; row < N; row++) {
      ORA[col][row] = (float) random()/32768.0;
    }
    ORB[col] = (float)random()/32768.0;
  }
  }
}

void reset_inputs(){
  int row, col;
  printf("\n reseting...\n");
  for (col = 0; col < N; col++) {
    for (row = 0; row < N; row++) {
      A[row][col] = ORA[row][col];
    }
    B[col] = ORB[col];
    X[col] = 0.0;
  }

}

/* Print input matrices */
void print_inputs() {
  int row, col;

  if (N < 1000) {
    printf("\nA =\n\t");
    for (row = 0; row < N; row++) {
      for (col = 0; col < N; col++) {
	printf("%5.2f%s", A[row][col], (col < N-1) ? ", " : ";\n\t");
      }
    }
    printf("\nB = [");
    for (col = 0; col < N; col++) {
      printf("%5.2f%s", B[col], (col < N-1) ? "; " : "]\n");
    }
  }
}

void  serialgauss(){
  int norm, row, col;  /* Normalization row, and zeroing
			* element row and col */
  float multiplier;
  
  printf("Computing serially.\n");

  /* Gaussian elimination */
  
  for (norm = 0; norm < N - 1; norm++) {
    
    // int num = N - norm;
    

    {

      //printf("%f ", A[norm][norm]);

      for (row = norm + 1; row < N; row++) {

        multiplier = A[row][norm] / A[norm][norm];

        for (col = norm; col < N; col++) {
            A[row][col] -= A[norm][col] * multiplier;
        }
        B[row] -= B[norm] * multiplier;
      }
  }
  }
  /* (Diagonal elements are not normalized to 1.  This is treated in back
   * substitution.)
   */
  /* Back substitution */
  for (row = N - 1; row >= 0; row--) {
    X[row] = B[row];
    for (col = N-1; col > row; col--) {
      X[row] -= A[row][col] * X[col];
    }
    X[row] /= A[row][row];
    //printf("%5.2f ", X[row]);
  }

}

void ompgauss() {
  int norm, row, col;  /* Normalization row, and zeroing
			* element row and col */
  float multiplier;
  //doneflag[0] = 1;
  
  printf("Computing using omp.\n");

  /* Gaussian elimination */
  
#pragma omp parallel private(row, col, multiplier, norm) num_threads(procs)
	{
		for (norm = 0; norm < N - 1; norm++) {
                       #pragma omp for schedule(static,1)
			for (row = norm + 1; row < N; row++) {
			  
			  multiplier = A[row][norm]/A[norm][norm];
				for (col = norm; col < N; col++) {
					A[row][col] -= A[norm][col] * multiplier;
				}
				B[row] -= B[norm] * multiplier;
			}
		}
	}
  nk_vc_printf("I am done\n");
  /* (Diagonal elements are not normalized to 1.  This is treated in back
   * substitution.)
   */
  /* Back substitution */
  for (row = N - 1; row >= 0; row--) {
    X[row] = B[row];
    for (col = N-1; col > row; col--) {
      X[row] -= A[row][col] * X[col];
    }
    X[row] /= A[row][row];
  }
}


#define TIME() (double)nk_sched_get_realtime();
static int handle_omptest (char * buf, void * priv)
{
    
    int seed, size, np;

    if ((sscanf(buf,"omptest %d %d %d",&seed,&size,&np)!=3))     { 
        nk_vc_printf("Don't understand %s please input seed, matrix size and nprocs\n",buf);
        return -1;
    }
    nk_rand_seed(seed);
    N = size;
    procs = np;
    nk_vc_printf("seed %d, size, %d, nprocs: %d\n", seed, N, procs);
    
    initialize_inputs();
    reset_inputs();
    // print_inputs();

    //unsigned mxcsr;
    //__asm__ volatile("ldmxcsr %0"::"m"(*&mxcsr):"memory");
    //printf("ld %04x \n", mxcsr);
    //mxcsr = mxcsr ^ 0x0200;
    //printf("st %08x \n", mxcsr);
    //__asm__ volatile("stmxcsr %0"::"m"(*&mxcsr):"memory");
    // __asm__ volatile("ldmxcsr %0"::"m"(*&mxcsr):"memory");
    //printf("ld %08x \n", mxcsr);
 
    double start = TIME();
    ompgauss();
    double  end = TIME();
    double  omp = end-start;
    nk_vc_printf("openmp done %lf\n", omp);
    float OMP[N];
    for(int row =0; row<N; row++){
      OMP[row] = X[row];
    }

    reset_inputs();
    start = TIME();
    serialgauss();
    end = TIME();
    double serial = end-start; 
    nk_vc_printf("serial done %lf\n", serial);
    float difference = 0.0;
    for(int row =0; row<N; row++){
      difference += (OMP[row]- X[row]);
    }

    nk_vc_printf("OMP difference %f speed up %f !\n", difference, serial/omp);
    return 0;

}


static struct shell_cmd_impl omptest_impl = {
    .cmd      = "omptest",
    .help_str = "omptest seed size np (openmp Gaussian elimination test)",
    .handler  = handle_omptest,
};
nk_register_shell_cmd(omptest_impl);

