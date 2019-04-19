#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>

using namespace std;

int main()
{
    int p[2];

    if (pipe(p) < 0) exit(1);

    pid_t x = fork();

    if (x == 0)
    {
        char inbuff[8];

        close(p[1]);
        read(p[0], inbuff, 6);
        cout << inbuff << endl;
        read(p[0], inbuff, 8);
        cout << inbuff << endl;
        close(p[0]);
    }
    else
    {
        close(p[0]);
        write(p[1], "Hello", 6);
        write(p[1], "It's me!", 8);
        close(p[1]);
        wait(0);
    }
    
    return 0;
}