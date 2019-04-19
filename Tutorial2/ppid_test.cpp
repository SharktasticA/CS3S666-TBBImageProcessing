#include <iostream>
#include <unistd.h>
#include <stdlib.h>

using namespace std;

int main(int argc, const char* argv[])
{
	if (argc > 1)
		cout << "Called by: " << argv[1] << endl;

	cout << "PID Test: " << getpid() << endl;
	cout << "PPID Test: " << getppid() << endl;

	int input;
	cout << "Enter a value: ";
	cin >> input;
	
	sleep(1);
	exit(input);
	return 0;
}