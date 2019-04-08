/*
 * This test program makes sure that when exec is done on a non-leader thread
 * in a threadgroup, the parent is not notified. The parent is notified only
 * after the leader exits. If the parent is notified too soon, then the test
 * fails.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>

#define CHILD_MIN_WAIT 4

void *t_func(void *priv)
{
    char waittime[10];

    printf ("Child Thread: starting. pid %d tid %d ; and sleeping\n", getpid(), syscall(SYS_gettid));
    printf ("Child Thread: doing exec of sleep\n");
    sprintf(waittime, "%d", CHILD_MIN_WAIT);
    execl("/bin/sleep", "sleep", waittime, (char *)NULL);
    printf ("Child Thread: DONE. pid %d tid %d\n", getpid(), syscall(SYS_gettid));
    /* the function must return something - NULL will do */
    return NULL;
}

int main (int argc, char **argv)
{
    int i = 0;
    long sum;
    int pid;
    int status, ret;
    pthread_t t1;
    time_t prog_start = time(NULL);

    printf ("Parent: pid: %d\n", getpid());

    pid = fork();

    if (pid == 0) {

        printf ("Child: starting. pid %d tid %d\n", getpid(), syscall(SYS_gettid));

        pthread_create(&t1, NULL, t_func, NULL);

        // Exec in the non-leader thread will destroy the leader 
        // immediately. If the wait in the parent returns too soon,
        // the test fails.
        while(1);

        exit(0);
    }

    // I am the parent
    printf ("Parent: Waiting for Child (%d) to complete.\n", pid);

    // Replace with pidfd_wait
    if ((ret = waitpid (pid, &status, 0)) == -1)
         printf ("parent:error\n");

    if (ret == pid)
        printf ("Parent: Child process waited for.\n");

    time_t prog_time = time(NULL) - prog_start;

    printf("Time waited for child: %lu\n", prog_time);

    if (prog_time < CHILD_MIN_WAIT || prog_time > CHILD_MIN_WAIT + 2)
        printf("TEST FAIL\n");
    else
        printf("TEST PASS\n");
}
