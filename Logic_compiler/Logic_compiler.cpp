// Logic_compiler.cpp: definisce il punto di ingresso dell'applicazione.
//

#include "Logic_compiler.h"
#include "file_manager.h"
#include "Alloc.h"
#include "Alloc_optimized.h"

using namespace std;

int main()
{
	cout << "Hello CMake." << endl;
	alloc_test();
	OsPagedVector<int> v(34, -2);
	v[12] = 254;
	for (auto i : v) {
		std::cout << i << "\n";
	}
	return 0;
}
