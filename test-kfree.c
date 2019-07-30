#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

int main(int argc, char *argv[])
{
  int fd[100];
  int i;
  int num_of_interations = 1000000;
  int timerfd_per_interation = 10;

  if (argc > 1) {
    if (argc != 3) {
      fprintf(stderr, "Usage: %s num_of_interation "
          "timerfd_per_iterations\n", argv[0]);
      exit(1);
    } else {
      num_of_interations = atoi(argv[1]);
      timerfd_per_interation = atoi(argv[2]);
      if (timerfd_per_interation > 100) {
        timerfd_per_interation = 100;
      }
    }
  }

  while (num_of_interations--) {
    for (i = 0; i < timerfd_per_interation; i++) {
      fd[i] = timerfd_create(CLOCK_REALTIME, 0);
    }

    for (i = 0; i < timerfd_per_interation; i++) {
      close(fd[i]);
    }
  }
}
