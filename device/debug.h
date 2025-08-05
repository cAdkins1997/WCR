#pragma once

#include "../common.h"

#include <iostream>
#include <sstream>

VKAPI_ATTR vk::Result VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
        vk::Instance instance,
        const vk::DebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const vk::AllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pMessenger);

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
        vk::Instance instance,
        vk::DebugUtilsMessengerEXT messenger,
        vk::AllocationCallbacks const *pAllocator);

VKAPI_ATTR vk::Bool32 VKAPI_CALL debugMessageFunc(
        vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
        const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *);
