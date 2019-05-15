#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

int main(void)
{
    if (mknod("myPipe", S_IFIFO | 0644, 0) < 0)
        cout << "Error" << endl;

    return 0;
}