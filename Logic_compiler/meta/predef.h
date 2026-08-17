#pragma once

#if 0
#define RELEASE
#endif // RELEASE MODE

#ifndef RELEASE //debug mode only
#include <cassert>
#define DBAssert(expr) assert(expr)
#else //release mode only
#define DBAssert(expr) ((void)0)
#endif

