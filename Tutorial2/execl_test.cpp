#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

int main(void)
{
    pid_t x = fork();

    if (x == 0)
    {
        cout << "Child PID: " << getpid() << endl;
        cout << "Child PPID: " << getppid() << endl;
        execl("./PPIDTest", "PPIDTest", "ExeclTest", NULL);
    }
    else cout << "Parent PID: " << getpid() << endl; 

    int status;
    pid_t child = wait(&status);

    if (child == x) cout << "Child terminated and returned: " << WEXITSTATUS(status) << endl; 

    return 0;
}