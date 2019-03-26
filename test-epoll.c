#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define __NR_pidfd_wait 428

#define MAXEVENTS 64

void print_exit(int wfd, void *buf)
{
    siginfo_t *si = (siginfo_t *)((char *)buf + sizeof(int));

    printf("PARENT: read returned, buf on fd %d: exit_state=%d\n\n", wfd, *(int *)buf);
    printf("PARENT: read returned, buf on fd %d: si_signo=%d\n\n", wfd, si->si_signo);
    printf("PARENT: read returned, buf on fd %d: si_code=%d\n\n", wfd,  si->si_code);
    printf("PARENT: read returned, buf on fd %d: si_errno=%d\n\n", wfd, si->si_errno);
    printf("PARENT: read returned, buf on fd %d: si_status=%d\n\n", wfd, si->si_status);
}

int
main (int argc, char *argv[])
{
    
    struct epoll_event event, event2;
    struct epoll_event *events;
    int pfds[2];
    char buf[30];
    int cpid, wfd;
    char path[100];

    pipe(pfds);
    
    cpid = fork();
    if (!cpid) {
        printf("CHILD: sleeping\n\n");
        fflush(stdout);
        sleep(3);
        printf("CHILD: writing to the pipe\n\n");
        write(pfds[1], "test", 5);
        fflush(stdout);
        sleep(3);
        printf("CHILD: exiting\n\n");
        fflush(stdout);
        exit(44);
    } else {
        int efd, n, s, pidfd;

        sprintf(path, "/proc/%d/", cpid);
        pidfd = open(path, O_DIRECTORY);
        printf("PARENT: child pid fd %d\n\n", pidfd);
        fflush(stdout);

        wfd = syscall(__NR_pidfd_wait, pidfd, 0);
        printf("PARENT: child's wfd %d\n\n", wfd);
        fflush(stdout);

        // Reading child
        char buf[100];

        // should block
        printf("PARENT: about to do blocking read on fd %d\n\n", wfd);
        fflush(stdout);
        read(wfd, buf, 100);

        print_exit(wfd, buf);
        fflush(stdout);

        // create epoll instance.
        efd = epoll_create1 (0);

        // allocate events array
        events = calloc (MAXEVENTS, sizeof event);

        // register read end of pipe using edge-triggered mode with ONESHOT.
        event.data.fd = pfds[0];
        event.events = EPOLLIN;
        s = epoll_ctl (efd, EPOLL_CTL_ADD, pfds[0], &event);

        printf("PARENT: done first EPOLL_ADD\n\n");
        fflush(stdout);

        event2.data.fd = wfd;
        event2.events = EPOLLIN;
        s = epoll_ctl (efd, EPOLL_CTL_ADD, wfd, &event2);

        printf("PARENT: done second EPOLL_ADD\n\n");
        fflush(stdout);

        // wait on read-end becoming readable.
        printf("PARENT: waiting on pipe and exit fd\n\n");
        fflush(stdout);

        while (1) {
            printf("PARENT: doing epoll again\n\n");

            n = epoll_wait (efd, events, MAXEVENTS, -1);
            printf("PARENT: epoll_wait success; n=%d.\n\n", n);
            fflush(stdout);

            for (int i = 0; i < n; i++) {
                char buf[100];
                read(events[i].data.fd, buf, 100);

                if (events[i].data.fd == wfd) {
                    print_exit(wfd, buf);
                } else {
                    printf("PARENT: read buf on fd %d: %s\n\n", events[i].data.fd, buf);
                }

                fflush(stdout);
            }

            printf("PARENT: about to do wait()\n\n");
            fflush(stdout);
            sleep(1);

            wait(NULL);
            printf("PARENT: wait returned, child reaped!\n\n");

            /*
               char buf2[100];
               read(wfd, buf2, 100);
               printf("PARENT TMP: read buf directly on fd %d: %s\n\n", wfd, buf2);
               fflush(stdout);
            */
        }
    }
    return 0;
    
}
