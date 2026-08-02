#include "Alloc.h"
#include <iostream>
void alloc_test() {

	using SparseVectorStd = SparseVector<uint8_t>;
	SparseVectorStd a = SparseVectorStd(UINT16_MAX, 254);
}