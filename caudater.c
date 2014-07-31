#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/inotify.h>

int main(int argc, char *argv[])
{
    unsigned char buf[1024];
    FILE *f = fopen(argv[1], "r");
    
    int fd = fileno(f);
    int ifd = inotify_init();
    int iwd = inotify_add_watch(ifd, argv[1], IN_MODIFY);
    struct inotify_event event;

    while(1) {
        read(ifd, &event, sizeof(event));
        if (event.mask & IN_MODIFY) {
            size_t l = read(fd, buf, sizeof(buf));
            buf[l] = '\0';
            printf("%s", buf);
            fflush(stdout);
        }
    }

    return 0;
}
