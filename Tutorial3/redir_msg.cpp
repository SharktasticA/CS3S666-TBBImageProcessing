#include <iostream>
#include <unistd.h>
#include <stdlib.h>

using namespace std;

int main(int argc, const char* argv[])
{
    string msg;

    cin >> msg;
    cout << "My message: " << msg << endl;

    exit(0);
}