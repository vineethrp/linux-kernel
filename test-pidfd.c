#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define __NR_pidfd_wait 428

int main(int argc, char **argv) 
{
    char path[100];
    int wfd, procfd, pidfd, pid = getpid();

    if (argc > 1)
	pid = atoi(argv[1]);
    
    printf("my pid %d\n", pid);

    sprintf(path, "/proc/%d/", pid);
    pidfd = open(path, O_DIRECTORY);
    printf("pid fd %d\n", pidfd);

    wfd = syscall(__NR_pidfd_wait, pidfd);
    printf("wfd %d\n", wfd);

    while(1) {
	char buf[100];
	sleep(2);
	read(wfd, buf, 100);
	printf("read: %s\n", buf);
    }
    return 0;
}
