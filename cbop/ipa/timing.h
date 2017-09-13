#include <time.h>
#include <strings.h>

#define TIME_TO_SEC(x) (((double) x) / 1000000000.0)
#define TIME_TO_MS(x) (((double) x) / 1000000.0)

// call this function to start a nanosecond-resolution timer
static inline struct timespec timer_start(){
  struct timespec start_time;
#ifdef COLLECT_STATS
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time);
#else
  bzero(&start_time, sizeof(struct timespec));
#endif
  return start_time;
}

// call this function to end a timer, returning nanoseconds elapsed as a long
static inline unsigned long timer_end(struct timespec start_time){
#ifdef COLLECT_STATS
  struct timespec end_time;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time);
  unsigned long diffInNanos = end_time.tv_nsec - start_time.tv_nsec;
  return diffInNanos;
#else
  return 0;
#endif
}
