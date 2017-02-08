#ifndef _ISOCORE
#define _ISOCORE

/*
  Convert this core into an isolated core
  Execution begins in a *copy* of the code
  with a distinct stack.

  Unless there is an error in startup, the
  nk_isolate() function will never return. 
  Also, the core will never again take
  another interrupt.

  codesize+stacksize must be smaller than
  the cache in which will be isolated

  The code must be position-independent, since
  we will relocate it.   If the code touches
  any global variable or the heap, isolation
  bets are off.

*/

int nk_isolate(void (*code)(void *arg),
	       uint64_t codesize,
	       uint64_t stacksize,
	       void     *arg);

#endif
