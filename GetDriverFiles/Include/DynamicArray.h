#pragma once

#include <stdint.h>

typedef struct  {
	void* pData;
	size_t ElementSize;
	size_t UsedSize;
	size_t AllocatedSize;
} dynamic_array;

void DaInitialize(dynamic_array* pDynamicArray, size_t ElementSize);
void DaFree(dynamic_array* pDynamicArray);
void DaResize(dynamic_array* pDynamicArray, size_t Size);
