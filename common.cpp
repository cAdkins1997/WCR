
#include "common.h"

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>


void vk_check(const vk::Result result,  const char* outputMessage) {
#ifdef DEBUG
    if (result != vk::Result::eSuccess)
        throw std::runtime_error((outputMessage + to_string(result) + '\n'));
#endif
}
