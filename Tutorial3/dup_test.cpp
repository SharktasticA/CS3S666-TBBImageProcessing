#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

int main(void)
{
    int pid1, pid2;

    int myPipes[2];

    if (pipe(myPipes) < 0) exit(1);

    pid1 = fork();

    int duplicateFd = dup(myPipes[0]);
    char buffer[6];
    buffer[5] = '\0';

    if (pid1 == 0)
    {
        read(myPipes[0], buffer, 5);
        cout << "Child 1: " << buffer << endl;
    }
    else
    {
        pid2 = fork();

        if (pid2 != 0)
        {
            write(myPipes[1], "HelloWorld", 11);
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);
        }
        else
        {
            read(duplicateFd, buffer, 5);
            cout << "Child 2: " << buffer << endl;
        }
    }
    
    return 0;
}