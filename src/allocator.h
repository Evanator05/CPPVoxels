#pragma once

#include <stdint.h>
#include <vector>
#include <stdio.h>

typedef struct _Allocation {
    uint32_t index;
    uint32_t size;
} Allocation;

template <typename T>
class Allocator {
    public:
        Allocation allocateData(uint32_t size);
        void freeData(Allocation data);
        void allocateEmpty(uint32_t size);
        T* data(void);
        size_t size(void);

        T& operator[](size_t index) {
            return allocationData[index];
        }

        void printStatus(void);
    private:
        std::vector<T> allocationData;
        std::vector<Allocation> freeAllocationData;
};

template <typename T>
Allocation Allocator<T>::allocateData(uint32_t size) {
    Allocation allocateData{};
    allocateData.size = size;
    uint32_t allocSize = size;

    // the index that was found
    allocateData.index = UINT32_MAX;
    for (size_t i = 0; i < freeAllocationData.size(); i++) {
        Allocation empty = freeAllocationData[i];
        
        // if the allocation is bigger than the available space then skip it
        if (size > empty.size) continue;
        allocateData.index = empty.index;

        // resize and position the empty span
        freeAllocationData[i].index += size;
        freeAllocationData[i].size -= size;

        // if the allocation is the same size as the empty space remove the empty span
        if (size == empty.size)
            freeAllocationData.erase(freeAllocationData.begin()+i);
        
        // break out of the loop because we have found our spot
        break;
    }
    // if no space found allocate at the end of the vector
    if (allocateData.index == UINT32_MAX) {
        allocateData.index = allocationData.size();
        allocationData.resize(allocationData.size()+allocSize);
    }
    return allocateData;
}

template <typename T>
void Allocator<T>::freeData(Allocation allocationData) {
    Allocation emptySpan{};
    emptySpan.index = allocationData.index;
    emptySpan.size = allocationData.size;

    // insert the new span in the right array spot
    size_t insertPos = 0;
    while (insertPos < freeAllocationData.size() && freeAllocationData[insertPos].index < emptySpan.index) {
        insertPos++;
    }
    freeAllocationData.insert(freeAllocationData.begin() + insertPos, emptySpan);

    Allocation &pEmptySpan = freeAllocationData[insertPos];

    // attempt right merge
    if (insertPos + 1 < freeAllocationData.size()) {
        Allocation rightBlock = freeAllocationData[insertPos+1];
        if (pEmptySpan.index+pEmptySpan.size == rightBlock.index) {
            pEmptySpan.size += rightBlock.size;
            freeAllocationData.erase(freeAllocationData.begin()+insertPos+1);
        }
    }
    // attempt left merge
    if (insertPos > 0) {
        Allocation leftBlock = freeAllocationData[insertPos-1];
        if(pEmptySpan.index == leftBlock.index+leftBlock.size) {
            pEmptySpan.index = leftBlock.index;
            pEmptySpan.size += leftBlock.size;
            freeAllocationData.erase(freeAllocationData.begin()+insertPos-1);
        }
    }
}

template <typename T>
void Allocator<T>::allocateEmpty(uint32_t size) {
    allocationData.resize(size);
    freeAllocationData.clear();
    freeAllocationData.push_back({0, size});
}


template <typename T>
T* Allocator<T>::data(void) {
    return allocationData.data();
}

template <typename T>
size_t Allocator<T>::size(void) {
    return allocationData.size();
}

template <typename T>
void Allocator<T>::printStatus(void) {
    printf("Free Spans:\n");
    for (int i = 0; i < freeAllocationData.size(); i++) {
        printf("position:%u\nsize:%u\n", freeAllocationData[i].index, freeAllocationData[i].size);
    }
    printf("\n");
}