#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DATA 92
#define SEGMENT '7'
#define STOP 33
#define BUFFER 40

char *read_long_text(int fd, int *len_ptr);

int buf_contains_stop(const char *buf, const int buf_len, int *index);

int main(void) {

    int pfd[2];
    pipe(pfd);

    char buf[DATA];
    for (int i = 0; i < DATA; i++) {
        buf[i] = i + 34;
    }
    buf[70] = STOP;

    write(pfd[1], buf, DATA);
    printf("Buffer is the following:\n");
    printf("%.*s\n", DATA, buf);

    int long_len;
    char *head = read_long_text(pfd[0], &long_len);
    if (head) {
        printf("Received long message as follows:\n");
        printf("%.*s\n", long_len, head);
    }
}

char *read_long_text(int fd, int *len_ptr) {

    char buffer[BUFFER];
    int readnum = read(fd, buffer, BUFFER);
    printf("read %d bytes\n", readnum);
    if (readnum < 1) {
        *len_ptr = 0;
        return NULL;
    }

    char *ptr;
    char *nptr;
    int stop_len;
    if (buf_contains_stop(buffer, readnum, &stop_len)) {
        printf("buf contains stop at %d\n", stop_len);
        ptr = (char *) malloc(stop_len);
        memmove(ptr, buffer, stop_len);
        *len_ptr = stop_len;
    } else {
        printf("buf keeps going\n");
        int sub_len;
        nptr = read_long_text(fd, &sub_len);
        ptr = (char *) malloc(readnum + sub_len);
        memmove(ptr, buffer, readnum);
        memmove(ptr + readnum, nptr, sub_len);
        *len_ptr = readnum + sub_len;
    }

    return ptr;
}

int buf_contains_stop(const char *buf, const int buf_len, int *index) {
    // precondition for invalid arguments
    if (buf == NULL || buf_len < 0) {
        return 0;
    }

    // buffer is empty
    if (buf_len == 0) {
        if (index != NULL) *index = -1;
        return 0;
    }

    for (int i = 0; i < buf_len; i++) {
        if (buf[i] == STOP) {
            if (index != NULL) *index = i;
            return 1;
        }
    }

    if (index != NULL) *index = -1;
    return 0;
}
