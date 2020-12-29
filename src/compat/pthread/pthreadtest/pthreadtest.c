#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/libccompat.h>
#include <nautilus/random.h>
#include <nautilus/scheduler.h>
#include "../pthread.h"
#include "../semaphore.h"

#define ERROR(fmt, args...) ERROR_PRINT("pthreadtest: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("pthreadtest: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("pthreadtest: " fmt, ##args)

static inline uint16_t random()
{
    uint16_t t; 
    nk_get_rand_bytes((uint8_t *)&t,sizeof(t));
    return t;
} 

#define MAXN 5100  /* Max value of N */
static int  N;  /* Matrix size */
static int procs;  /* Number of processors to use */

/* Matrices and vectors */
static float A[MAXN][MAXN], B[MAXN], X[MAXN];
static float ORA[MAXN][MAXN], ORB[MAXN], ORX[MAXN];
/* A * X = B, solve for X */

//int seed;
/* Prototype */
static void gauss();  /* The function you will provide.
		* It is this routine that is timed.
		* It is called only on the parent.
		*/

//Initialize A and B (and X to 0.0s)
static void initialize_inputs() {
  int row, col;
 
  printf("\nInitializing...\n");
  #pragma omp parallel private(col, row) no_wait num_threads(8)
  for (col = 0; col < N; col++) {
    #pragma omp for schedule(dynamic)
    for (row = 0; row < N; row++) {
      ORA[row][col] = (float) random()/32768.0;
    }
    ORB[col] = (float)random()/32768.0;
    ORX[col] = 0.0;
  }
}

static void reset_inputs(){
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

static void print_state_hex() {
  int row, col;

  if (N < 100) {
    nk_vc_printf("\nA =\n\t");
    for (row = 0; row < N; row++) {
      for (col = 0; col < N; col++) {
	  nk_vc_printf("%08x%s  ", *(unsigned *)&(A[row][col]), (col < N-1) ? ", " : ";\n\t");
      }
    }
    nk_vc_printf("\nB = [");
    for (col = 0; col < N; col++) {
	nk_vc_printf("%08x%s  ", *(unsigned *)&(B[col]), (col < N-1) ? "; " : "]\n");
    }
    nk_vc_printf("\nX = [");
    for (col = 0; col < N; col++) {
	nk_vc_printf("%08x%s  ", *(unsigned *)&(X[col]), (col < N-1) ? "; " : "]\n");
    }
  }
}

static void print_state() {
  int row, col;
  
  if (N < 100) {
    nk_vc_printf("\nA =\n\t");
    for (row = 0; row < N; row++) {
      for (col = 0; col < N; col++) {
	nk_vc_printf("%f%s  ", A[row][col], (col < N-1) ? ", " : ";\n\t");
      }
    }
    nk_vc_printf("\nB = [");
    for (col = 0; col < N; col++) {
      nk_vc_printf("%f%s  ", B[col], (col < N-1) ? "; " : "]\n");
    }
    nk_vc_printf("\nX = [");
    for (col = 0; col < N; col++) {
      nk_vc_printf("%f%s  ", X[col], (col < N-1) ? "; " : "]\n");
    }
  }

  //  print_state_hex();
}



//pthread_barrier_t barrier;
static sem_t mutex; 
static sem_t barrier; 
static int count = 0;
static int wait = 0;

static int doneflag[MAXN] = {0};
static pthread_mutex_t lock;
static void* cal_zero(void* threadid);

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

static void pthreadgauss() {
  int norm, row, col;  /* Normalization row, and zeroing
			* element row and col */
  float multiplier;
  int numthreads = N-1;
  doneflag[0] = 1;
  pthread_t threads[procs];
  nk_vc_printf("Computing Using Pthread\n");
  pthread_mutex_init(&lock,NULL);;
  // sem_init(&mutex,0,1);
  // sem_init(&barrier,0,0);
  //pthread_barrier_init(&barrier, NULL, numthreads);
  for (int i=0; i < procs; i++)
  {
     pthread_create(&threads[i],NULL, &cal_zero, (void *)i);
  }

  for(int i=0; i < procs; i++)
  {
     pthread_join(threads[i], NULL); 
  }
  /* Back substitution */
  for (row = N - 1; row >= 0; row--) {
    X[row] = B[row];
    for (col = N-1; col > row; col--) {
      X[row] -= A[row][col] * X[col];
    }
    X[row] /= A[row][row];
  }
}

#define min(x, y) ((x) < (y) ? (x) : (y))

static void *cal_zero(void* threadid) {
  long id = (long) threadid;
  float multiplier;
  for (long norm = 0; norm < N-1 ; norm++){
    
    //busy wait with relinquish cpu
    while(1){
       pthread_mutex_lock(&lock);
     
       if( doneflag[norm] == 1){
        pthread_mutex_unlock(&lock);
        break;
       }else{
         pthread_mutex_unlock(&lock);
         nk_yield();
       }
    }
    for(int row = id; row < N; row+=procs){
        if(row <= norm)
            continue;
        multiplier = A[row][norm] / A[norm][ norm];
        for (int col = norm; col < N; col++) {
          A[row][col] -= A[norm][col] * multiplier;
        }
        B[row] -= B[norm] * multiplier;
        
        if(row == norm+1 ){
          //critical region
          pthread_mutex_lock(&lock);
          // for(int k=0;k<)
          //     nk_vc_printf("%d", doneflag);
          doneflag[norm+1] = 1;
          pthread_mutex_unlock(&lock);
        }
    }
  }
}


#define TIME() (double)nk_sched_get_realtime()/1e9;
static int handle_pthreadtest (char * buf, void * priv)
{

    int seed, size, np;

    if ((sscanf(buf,"ptest %d %d %d",&seed,&size,&np)!=3))     { 
        nk_vc_printf("Don't understand %s please input seed, matrix size and nprocs\n",buf);
        return -1;
    }
    nk_rand_seed(seed);
    N = size;
    procs = np;
    nk_vc_printf("seed %d, size, %d, nprocs: %d\n", seed, N, procs);
    initialize_inputs();
    reset_inputs();
    nk_vc_printf("state before pthread run\n");
    print_state();
    double start = TIME();
    pthreadgauss();
    double  end = TIME();
    nk_vc_printf("state after pthread run\n");
    print_state();
    double  omp = end-start;
    nk_vc_printf("openmp done %lf\n", omp);
     float OMP[N];
    for(int row =0; row<N; row++){
      OMP[row] = X[row];
    }

    reset_inputs();
    nk_vc_printf("state before serial run\n");
    print_state();
    start = TIME();
    serialgauss();
    end = TIME();
    nk_vc_printf("state after serial run\n");
    print_state();
    double serial = end-start; 
    nk_vc_printf("serial done %lf\n", serial);
    float difference = 0.0;
    for(int row =0; row<N; row++){
      difference += (OMP[row]- X[row]);
    }

    nk_vc_printf("pthread difference %f speed up %f !\n", difference, serial/omp);
    return 0;

}

static void handle_pthread_bulk_test(char * buf, void * priv){
    nk_rand_seed(100);
    for(int size=1000;size<6000;size+=1000){
      N =  size;
      initialize_inputs();
      reset_inputs();
      double start = TIME();
      serialgauss();
      double end = TIME();
      double serial = end-start; 
      nk_vc_printf("serial done %lf\n", serial);
      float difference = 0.0;

      for(int np=1; np<9;np++){	
       reset_inputs();
       procs = np;
       start = TIME();
       pthreadgauss();
        end = TIME();
       double  omp = end-start;
       nk_vc_printf("size %d nprocs %d pthread done %lf\n",size,np, omp);
       nk_vc_printf("pthread difference speed up %f !\n", serial/omp);
    }
    }
}

static struct shell_cmd_impl pthreadtest_impl = {
    .cmd      = "ptest",
    .help_str = "ptest seed size np (pthread Gaussian elimination test)",
    .handler  = handle_pthreadtest,
};
nk_register_shell_cmd(pthreadtest_impl);


static struct shell_cmd_impl pthreadbtest_impl = {
    .cmd      = "pbtest",
    .help_str = "pbtest (pthread bulk test)",
    .handler  = handle_pthread_bulk_test,
};
nk_register_shell_cmd(pthreadbtest_impl);
