#pragma once

#include <malloc.h>
#include <stdlib.h>

inline void* malloc_guarded(size_t size) {
	void* p = malloc(size);
	if (!p) abort();
	return p;
}

inline void* realloc_guarded(void* p, size_t size) {
	void* pNew = realloc(p, size);
	if (!pNew) abort();
	return pNew;
}
