

#include <nautilus/printk.h>
#include <nautilus/libccompat.h>

#include "test.h"
#include "gc.h"

#define COUNT 10000000

int realloc_test(void) {
  int i;
  unsigned long last_heap_size = 0;

  GC_INIT();

  for (i = 0; i < COUNT; i++) {
    int **p = GC_MALLOC(sizeof(int *));
    int *q = GC_MALLOC_ATOMIC(sizeof(int));

    if (p == 0 || *p != 0) {
      printk("GC_malloc returned garbage (or NULL)\n");
      exit(1);
    }

    *p = GC_REALLOC(q, 2 * sizeof(int));

    if (i % 10 == 0) {
      unsigned long heap_size = (unsigned long)GC_get_heap_size();
      if (heap_size != last_heap_size) {
        printk("Heap size: %lu\n", heap_size);
        last_heap_size = heap_size;
      }
    }
  }
  printk("Succesfully passed realloc test!\n");
  return 0;
}
