#include "CStringView.h"
#include "Osvector.h"

namespace cc {

	// Explicit template instantiations compiled into this translation unit
	//string view
	template class CStrViewImpl<uint8_t>;
	template class CStrViewImpl<uint16_t>;
	template class CStrViewImpl<uint32_t>;
	template class CStrViewImpl<uint64_t>;

	//OsPagedVector
	template class cc::osvector::OsPagedVectorImpl<char>;

} // namespace cc