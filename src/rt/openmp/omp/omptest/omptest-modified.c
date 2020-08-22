#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/libccompat.h>
#include <nautilus/random.h>
#include <nautilus/math.h>
#include "gomptestdata.h"

#define ERROR(fmt, args...) ERROR_PRINT("omptest: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("omptest: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("omptest: " fmt, ##args)

#define ALLOC(size) ({ void *__fake = malloc(size); if (!__fake) { ERROR("Failed to allocate %lu bytes\n",size); } __fake; })

static inline uint16_t random()
{
    uint16_t t;
    nk_get_rand_bytes((uint8_t *)&t,sizeof(t));
    return t;
}

#define MAXN 5100  /* Max value of N */

//static int N;  /* Matrix size */
static int procs;  /* Number of processors to use */

/* Matrices and vectors */
//static volatile float A[MAXN][MAXN], B[MAXN], X[MAXN];
//static volatile float ORA[MAXN][MAXN], ORB[MAXN], ORX[MAXN];

static float **A;
static float *B, *X, *ORX;
/* A * X = B, solve for X */

static int seed;
/* Prototype */
static void gauss();  /* The function you will provide.
		* It is this routine that is timed.
		* It is called only on the parent.
		*/

int ISNAN(float f)
{
    uint32_t val = *(uint32_t*)&f;
    val &= 0x7fffffff;

    // S EEEEEEEE FFFFFFFFFFFFFFFFFFFFFFF
    //   11111111  => special    FFF=0 => infinity,  any F not zero => nan
    
    return ((val>>23)==0xff) && (val & 0x7fffff != 0);
}


uint32_t read_mxcsr()
{
    uint32_t val;
    asm volatile ("stmxcsr %[_m]" : [_m] "=m" (val) : : "memory");
    return val;
}
 
void write_mxcsr(uint32_t val)
{
    asm volatile ("ldmxcsr %[_m]" :: [_m] "m" (val) : "memory");
}

/* Initialize A and B (and X to 0.0s) */
static void initialize_inputs() {
  int row, col;
  //ORA = ALLOC(N * sizeof(float *));
  A = ALLOC (N * sizeof(float *));
  for(int i = 0; i < N ; i++ ){
    //ORA[i] = ALLOC(N * sizeof(float));
    A[i] = ALLOC(N * sizeof(float));
  }

  //ORB = ALLOC(N * sizeof(float));
  B = ALLOC(N * sizeof(float));
  ORX = ALLOC(N * sizeof(float));
  X = ALLOC(N * sizeof(float));
 
  /* nk_vc_printf("\nInitializing...\n"); */
  /* //   #pragma omp parallel num_threads(8) */
  /* { */
  /* //  #pragma omp for private(row,col) schedule(static,1) nowait */
  /*   for (col = 0; col < N; col++) { */
   
  /*   for (row = 0; row < N; row++) { */
  /*     ORA[col][row] = (float) random()/32768.0; */
  /*   } */
  /*   ORB[col] = (float)random()/32768.0; */
  /* } */
  /* } */
}

static void reset_inputs(){
  int row, col;
  nk_vc_printf("\n reseting...\n");
  for (col = 0; col < N; col++) {
    for (row = 0; row < N; row++) {
      A[row][col] = ORA[row][col];
    }
    B[col] = ORB[col];
    X[col] = 0.0;
  }

}

static void print_state() {
  int row, col;

  nk_vc_printf("A=%p, B=%p, X=%p\n", A, B, X);
  
  if (N < 3000) {
    nk_vc_printf("\nA =\n\t");
    for (row = 0; row < N; row++) {
      for (col = 0; col < N; col++) {
    	nk_vc_printf("%f%s  ", A[row][col], (col < N-1) ? ", " : ";\n\t");
      }
    }
    nk_vc_printf("\nB = [");
    for (col = 0; col < N; col++) {
      nk_vc_printf("%5.2f%s", B[col], (col < N-1) ? "; " : "]\n");
    }
    nk_vc_printf("\nX = [");
    for (col = 0; col < N; col++) {
      nk_vc_printf("%f%s  ", X[col], (col < N-1) ? "; " : "]\n");
    }
  }
}

void check_state(int after) {
  int row, col;

  //  nk_vc_printf("A=%p, B=%p, X=%p\n", A, B, X);
  nk_vc_printf("check_init_state(after=%d) mxcsr=%08x\n",after,read_mxcsr());
  
  if (N < 3000) {
      nk_vc_printf("check A\n");
      if (!after) { 
	  for (row = 0; row < N; row++) {
	      for (col = 0; col < N; col++) {
		  if (ISNAN(ORA[row][col])) {
		      nk_vc_printf("found ORA nan at %d %d\n",row,col);
		  }
		  if (ISNAN(A[row][col])) {
		      nk_vc_printf("found A nan at %d %d\n",row,col);
		  }
		  if (A[row][col] != ORA[row][col]) {
		      nk_vc_printf("found flt A != ORA at %d  %d (%f (%08x) addr %p <-> %f (%08x) addr %p mxcsr=%08x intr=%lu excp=%l\n",row,col,A[row][col],*(uint32_t*)&A[row][col], &A[row][col], ORA[row][col], *(uint32_t*)&ORA[row][col], &ORA[row][col],read_mxcsr(),get_cpu()->interrupt_count,get_cpu()->exception_count);
		  }
		  if (*(uint32_t *)&A[row][col] != *(uint32_t *)&ORA[row][col]) {
		      nk_vc_printf("found int A != ORA at %d  %d (%f (%08x) addr %p <-> %f (%08x) addr %p\n",row,col,A[row][col],*(uint32_t*)&A[row][col], &A[row][col], ORA[row][col], *(uint32_t*)&ORA[row][col], &ORA[row][col]);
		  }
	      }
	  }
      }
	  
      nk_vc_printf("check B\n");

      if (!after) { 
	  for (col = 0; col < N; col++) {
	      if (ISNAN(ORB[col])) {
		  nk_vc_printf("found ORB nan at %d\n",col);
		  continue;
	      }
	      if (ISNAN(B[col])) {
		  nk_vc_printf("found B nan at %d\n",col);
		  continue;
	      }
	      if (B[col] != ORB[col]) {
		  nk_vc_printf("found B != ORB at %d\n",col);
		  continue;
	      }
	  }
      }
	  
      if (!after) { 
	  for (col = 0; col < N; col++) {
	      if (ISNAN(X[col])) {
		  nk_vc_printf("found X nan at %d\n",col);
		  continue;
	      }
	      if (X[col] != 0) {
		  nk_vc_printf("found X !=0 at %d\n",col);
		  continue;
	      }
	  }
      }
  }
}
  

static void  serialgauss(){
  int norm, row, col;  /* Normalization row, and zeroing
			* element row and col */
  float multiplier;
  
  nk_vc_printf("Computing serially.\n");

  /* Gaussian elimination */
  
  for (norm = 0; norm < N - 1; norm++) {
    
    // int num = N - norm;
    

    {

      //nk_vc_printf("%f ", A[norm][norm]);

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
    //nk_vc_printf("%5.2f ", X[row]);
  }

}

static void ompgauss() {
  int norm, row, col;  /* Normalization row, and zeroing
			* element row and col */
  float multiplier;
  //doneflag[0] = 1;
  
  nk_vc_printf("Computing using omp.\n");

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
    setenv("OMP_WHATEVER","53");
    int seed, size, np;


#if 1 // BUG
    nk_vc_printf("Initializing\n");
    nk_vc_printf("mxcsr=%08x\n",read_mxcsr());
    initialize_inputs();

    nk_vc_printf("Round 1\n");
    reset_inputs();

    float *SGAUSS;
    SGAUSS = ALLOC(N*sizeof(float));

    int nan_output_r1=0;
    
    reset_inputs();
    //    while (1) { check_state(0); }
    
    check_state(0);
    serialgauss();
    check_state(1);
    
    for(int row = 0; row < N; row++){
	if (ISNAN(X[row])) {
	    nan_output_r1 = 1;
	}
	SGAUSS[row] = X[row];
    }

    if (nan_output_r1) {
	nk_vc_printf("Encountered nan in r1 solution\n");
    }

    int nan_output_r2=0;

    reset_inputs();
    check_state(0);
    serialgauss();
    check_state(1);

    float diff2 = 0.0;
    for(int row =0; row<N; row++){
	if (ISNAN(X[row])) {
	    nan_output_r2 = 1;
	}
	diff2 += (SGAUSS[row]- X[row]);
    }

    if (nan_output_r2) {
	nk_vc_printf("Encountered nan in r2 solution\n");
    }

    nk_vc_printf("Same serial diff %f\n",diff2);
    

#else 
    if ((sscanf(buf,"omptest %d %d %d",&seed,&size,&np)!=3))     { 
        nk_vc_printf("Don't understand %s please input seed, matrix size and nprocs\n",buf);
        return -1;
    }
    nk_rand_seed(seed);
    //N = size;
    procs = np;
    nk_vc_printf("seed %d, size, %d, nprocs: %d\n", seed, N, procs);
    
    
    initialize_inputs();
    reset_inputs();
    nk_vc_printf("state before openmp run\n");
    //print_state();

    //unsigned mxcsr;
    //__asm__ volatile("ldmxcsr %0"::"m"(*&mxcsr):"memory");
    //nk_vc_printf("ld %04x \n", mxcsr);
    //mxcsr = mxcsr ^ 0x0200;
    //nk_vc_printf("st %08x \n", mxcsr);
    //__asm__ volatile("stmxcsr %0"::"m"(*&mxcsr):"memory");
    // __asm__ volatile("ldmxcsr %0"::"m"(*&mxcsr):"memory");
    //nk_vc_printf("ld %08x \n", mxcsr);

    float *SGAUSS;
    SGAUSS = ALLOC(N*sizeof(float));
    reset_inputs();
    serialgauss();
    for(int row = 0; row < N; row++){
      SGAUSS[row] = X[row];
    }
   

    /* float OMP2[N]; */
    /* for(int row = 0; row < N; row++){ */
    /*   OMP2[row] = X[row]; */
    /* } */
    /* reset_inputs(); */
    /* ompgauss(); */
 
    /* double start = TIME(); */
    /* ompgauss(); */
    /* double  end = TIME(); */
    /* nk_vc_printf("state after openmp run\n"); */
    /* //print_state(); */
    /* double  omp = end-start; */
    /* nk_vc_printf("openmp done %lf\n", omp); */
    /* float OMP[N]; */
    /* /\* float *OMP; *\/ */
    /* /\* OMP = ALLOC(N * sizeof(float)); *\/ */
    /* for(int row =0; row<N; row++){ */
    /*   OMP[row] = X[row]; */
    /* } */

    reset_inputs();
    /* nk_vc_printf("state before serial run\n"); */
    /* //print_state(); */
    /* start = TIME(); */
    serialgauss();
    /* end = TIME(); */
    /* nk_vc_printf("state after serial run\n"); */
    /* //print_state(); */
    /* double serial = end-start;  */
    /* nk_vc_printf("serial done %lf\n", serial); */
    /* float difference = 0.0; */
    /* for(int row =0; row<N; row++){ */
    /*   difference += (OMP[row]- X[row]); */
     /* } */

    /* nk_vc_printf("OMP difference %f speed up %f !\n", difference, serial/omp); */
   
    float diff2 = 0.0;
    for(int row =0; row<N; row++){
      diff2 += (SGAUSS[row]- X[row]);
    }
    nk_vc_printf("Same serial diff %f\n",diff2);

#endif
    return 0;

}


static struct shell_cmd_impl omptest_impl = {
    .cmd      = "omptest",
    .help_str = "omptest seed size np (openmp Gaussian elimination test)",
    .handler  = handle_omptest,
};
nk_register_shell_cmd(omptest_impl);

