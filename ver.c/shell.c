#include "user.h"

void get_cmdline(char *buf, size_t size) {
    for (size_t i = 0;; i++) {
        char ch = getchar();
        putchar(ch);
        if (i == size - 1) {
            printf("command line too long\n");
            buf[0] = '\0';
            return;
        } else if (ch == '\r') {
            printf("\n");
            buf[i] = '\0';
            return;
        } else {
            buf[i] = ch;
        }
    }
}

void main(void) {
    while (1) {
        printf("> ");
        char cmdline[128];
        get_cmdline(cmdline, sizeof(cmdline));
        if (strcmp(cmdline, "hello") == 0)
            printf("Hello world from shell!\n");
        else if (strcmp(cmdline, "exit") == 0)
            exit();
        else if (strcmp(cmdline, "readfile") == 0) {
            char buf[128];
            int len = readfile("hello.txt", buf, sizeof(buf));
            buf[len] = '\0';
            printf("%s\n", buf);
        } else if (strcmp(cmdline, "writefile") == 0)
            writefile("hello.txt", "Hello, virtio disk!\n", 22);
        else
            printf("unknown command: %s\n", cmdline);
    }
}
