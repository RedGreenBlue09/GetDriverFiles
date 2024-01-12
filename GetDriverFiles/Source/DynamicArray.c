#include <assert.h>
#include <limits.h>

#include "DynamicArray.h"
#include "GuardedMalloc.h"

#define INITIAL_SIZE 16

void DaInitialize(dynamic_array* pDynamicArray, size_t ElementSize) {
	assert(pDynamicArray != NULL);
	pDynamicArray->pData = malloc_guarded(INITIAL_SIZE * ElementSize);
	pDynamicArray->ElementSize = ElementSize;
	pDynamicArray->UsedSize = 0;
	pDynamicArray->AllocatedSize = INITIAL_SIZE;
}

void DaFree(dynamic_array* pDynamicArray) {
	assert(pDynamicArray != NULL);
	assert(pDynamicArray->pData != NULL);

	free(pDynamicArray->pData);
	pDynamicArray->pData = NULL;
	pDynamicArray->ElementSize = 0;
	pDynamicArray->UsedSize = 0;
	pDynamicArray->AllocatedSize = 0;
}

// Micro optimization
static size_t NextPowerOf2(size_t X) {
	--X;
	// Only Clang-cl unrolls this.
	// Anyway, that doesn't matter.
	size_t Mask = 1;
	while (Mask < (sizeof(X) * CHAR_BIT)) {
		X |= X >> Mask;
		Mask <<= 1;
	}
	return X + 1;
}

void DaResize(dynamic_array* pDynamicArray, size_t Size) {
	assert(pDynamicArray != NULL);
	assert(pDynamicArray->pData != NULL);

	if (Size <= (pDynamicArray->AllocatedSize / 2) || (Size > pDynamicArray->AllocatedSize)) {
		size_t NewAllocatedSize = NextPowerOf2(Size);
		if (NewAllocatedSize < INITIAL_SIZE)
			NewAllocatedSize = INITIAL_SIZE;
		pDynamicArray->pData = realloc_guarded(pDynamicArray->pData, NewAllocatedSize * pDynamicArray->ElementSize);
		pDynamicArray->AllocatedSize = NewAllocatedSize;
	}
	pDynamicArray->UsedSize = Size;
}
