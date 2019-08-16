#include <stdio.h>
#include <unistd.h>
#include <time.h>

int main(void) {

    time_t seconds;
    while (1) {
        sleep(1);
        seconds = time(NULL);
        printf("Time is %ld\n", seconds);
    }
}
