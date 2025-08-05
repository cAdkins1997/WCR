
#include "common.h"

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>


void vk_check(const vk::Result result, const std::string& outputString) {
#ifdef DEBUG
    if (result != vk::Result::eSuccess)
        throw std::runtime_error((outputString + ' ' + to_string(result) + '\n'));
#endif
}
