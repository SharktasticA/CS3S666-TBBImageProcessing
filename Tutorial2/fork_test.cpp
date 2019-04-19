#include <iostream>
#include <unistd.h>

using namespace std;

int main(void)
{
	pid_t x = fork();

	if (x == 0) cout << "I'm a child!" << endl;
	else cout << "I'm a parent!" << endl;

	return 0;
}
