/*
 * This test program makes sure that when a thread group
 * leader exits, but the group has other threads, only the last
 * exiting thread results in pidfd-wait returning. This is
 * the same behavior as wait(2).
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

#define CHILD_MIN_WAIT 2

static time_t *child_exit_secs;

void *t_func(void *priv)
{
    printf ("Child Thread: starting. pid %d tid %d ; and sleeping\n", getpid(), syscall(SYS_gettid));
    sleep(CHILD_MIN_WAIT);
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
    pthread_t t1, t2;

    child_exit_secs = mmap(NULL, sizeof *child_exit_secs, PROT_READ | PROT_WRITE, 
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    printf ("Parent: pid: %d\n", getpid());

    pid = fork();

    if (pid == 0) {

        // I am the child
        printf ("Child: starting. pid %d tid %d\n", getpid(), syscall(SYS_gettid));

        // Creating child threads
        pthread_create(&t1, NULL, t_func, NULL);
        pthread_create(&t2, NULL, t_func, NULL);

        *child_exit_secs = time(NULL);
        syscall(SYS_exit, 0);
    }

    // I am the parent
    printf ("Parent: Waiting for Child (%d) to complete.\n", pid);

    // Replace with pidfd_wait
    if ((ret = waitpid (pid, &status, 0)) == -1)
         printf ("parent:error\n");

    if (ret == pid)
        printf ("Parent: Child process waited for.\n");

    time_t since_child_exit = time(NULL) - *child_exit_secs;

    printf("Time since child exit: %lu\n", since_child_exit);

    if (since_child_exit < CHILD_MIN_WAIT || since_child_exit > CHILD_MIN_WAIT + 2)
        printf("TEST FAIL\n");
    else
        printf("TEST PASS\n");
}
