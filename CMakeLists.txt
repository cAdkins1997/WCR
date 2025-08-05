cmake_minimum_required(VERSION 3.31)
project(WCR)

set(CMAKE_CXX_STANDARD 26)

add_executable(WCR main.cpp
        common.h
        glmdefines.h
        common.cpp
        application.cpp
        application.h)

find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator HINTS "C:/Users/ulver/.vcpkg-clion/vcpkg/packages/vulkan-memory-allocator_x64-windows")
find_package(zstd CONFIG REQUIRED HINTS "C:/Users/ulver/.vcpkg-clion/vcpkg/packages/zstd_x64-windows")
find_package(Ktx CONFIG REQUIRED HINTS  "C:/Users/ulver/.vcpkg-clion/vcpkg/packages/ktx_x64-windows")
find_package(meshoptimizer CONFIG REQUIRED HINTS "C:/Users/ulver/.vcpkg-clion/vcpkg/packages/meshoptimizer_x64-windows")

add_subdirectory(include/glfw)
add_subdirectory(include/fastgltf)

set(IMGUI_PATH ${CMAKE_CURRENT_LIST_DIR}/include/imgui)
file(GLOB IMGUI_GLOB
        ${IMGUI_PATH}/imgui.h
        ${IMGUI_PATH}/imgui.cpp
        ${IMGUI_PATH}/imconfig.h
        ${IMGUI_PATH}/imgui_demo.cpp
        ${IMGUI_PATH}/imgui_draw.cpp
        ${IMGUI_PATH}/imgui_internal.h
        ${IMGUI_PATH}/imstb_rectpack.h
        ${IMGUI_PATH}/imstb_textedit.h
        ${IMGUI_PATH}/imstb_truetype.h
        ${IMGUI_PATH}/imgui_tables.cpp
        ${IMGUI_PATH}/imgui_widgets.cpp

        ${IMGUI_PATH}/backends/imgui_impl_glfw.h
        ${IMGUI_PATH}/backends/imgui_impl_glfw.cpp
        ${IMGUI_PATH}/backends/imgui_impl_vulkan.h
        ${IMGUI_PATH}/backends/imgui_impl_vulkan.cpp)

add_library("imgui" STATIC ${IMGUI_GLOB}
        device/resourcetypes.h
        device/device.cpp
        device/device.h
        device/debug.cpp
        device/debug.h
        device/context.cpp
        device/context.h
        commands.cpp
        commands.h)
target_include_directories("imgui" PUBLIC ${IMGUI_PATH})
target_link_libraries("imgui" PRIVATE glfw Vulkan::Vulkan)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

target_link_libraries(
        ${PROJECT_NAME}
        Vulkan::Vulkan
        glfw
        fastgltf::fastgltf
        imgui
        KTX::ktx
        meshoptimizer::meshoptimizer
)