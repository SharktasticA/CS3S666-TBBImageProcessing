#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

int main(void)
{
    close(0);
    int myPipe[2];

    if (pipe(myPipe) < 0) exit(-1);

    dup(myPipe[0]);

    int x = fork();

    if (x == 0)
    {
        if (execl("./RedirMsg", "RedirMsg", NULL) < 0)
            exit(-1);
    }
    else
    {
        close(0);
        write(myPipe[1], "HelloFriend ", 12);
        close(1);
        wait(NULL);
    }
    
    return 0;
}