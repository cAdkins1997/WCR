
#include "debug.h"

vk::Bool32 debugMessageFunc(const vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                            const vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
                            const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *) {
    std::ostringstream message;

    message << vk::to_string(messageSeverity) << ": " << vk::to_string(messageTypes) << ":\n";
    message << std::string("\t") << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
    message << std::string("\t") << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
    message << std::string("\t") << "message         = <" << pCallbackData->pMessage << ">\n";
    if (0 < pCallbackData->queueLabelCount) {
        message << std::string("\t") << "Queue Labels:\n";

        for (u32 i = 0; i < pCallbackData->queueLabelCount; i++)
            message << std::string("\t\t") << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
    }
    if (0 < pCallbackData->cmdBufLabelCount) {
        message << std::string("\t") << "CommandBuffer Labels:\n";

        for (u32 i = 0; i < pCallbackData->cmdBufLabelCount; i++)
            message << std::string("\t\t") << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
    }
    if (0 < pCallbackData->objectCount) {
        message << std::string("\t") << "Objects:\n";

        const auto callBackData = pCallbackData;

        for (u32 i = 0; i < pCallbackData->objectCount; i++) {
            message << std::string("\t\t") << "Object " << i << "\n";
            message << std::string("\t\t\t") << "objectType   = " << to_string(callBackData->pObjects[i].objectType) << "\n";
            message << std::string("\t\t\t") << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";

            if (pCallbackData->pObjects[i].pObjectName)
                message << std::string("\t\t\t") << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
        }
    }

    std::cout << message.str() << std::endl;
    return false;
}

void vkDestroyDebugUtilsMessengerEXT(vk::Instance instance, const vk::DebugUtilsMessengerEXT messenger, const vk::AllocationCallbacks *pAllocator) {
    const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
    if (func != nullptr)
        func(instance, messenger, *pAllocator);
}

vk::Result vkCreateDebugUtilsMessengerEXT(
    const vk::Instance instance,
    const vk::DebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const vk::AllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pMessenger)
{
    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));
    if (func != nullptr)
        return static_cast<vk::Result>(func(instance, *pCreateInfo, *pAllocator, pMessenger));
    return vk::Result::eErrorExtensionNotPresent;
}