cmake_minimum_required(VERSION 3.31)
project(WCR)

set(CMAKE_CXX_STANDARD 26)

add_executable(WCR main.cpp
        common.h
        glmdefines.h
        common.cpp
        application.cpp
        application.h
        camera.cpp
        camera.h
        commands.h
        commands.cpp
        device/resourcetypes.h
        device/debug.h
        device/debug.cpp
        device/device.h
        device/device.cpp
        device/context.cpp
        device/context.h
        pipelines/descriptors.h
        pipelines/descriptors.cpp
        pipelines/pipelines.h
        pipelines/pipelines.cpp
        scenes/scenemanager.cpp
        scenes/scenemanager.h
)

find_package(Vulkan REQUIRED)
find_package(zstd CONFIG REQUIRED HINTS "C:/Users/ulver/.vcpkg-clion/vcpkg/packages/zstd_x64-windows")
find_package(Ktx CONFIG REQUIRED HINTS  "C:/Users/ulver/.vcpkg-clion/vcpkg/packages/ktx_x64-windows")
find_package(meshoptimizer CONFIG REQUIRED HINTS "C:/Users/ulver/.vcpkg-clion/vcpkg/packages/meshoptimizer_x64-windows")
find_package(TBB CONFIG REQUIRED HINTS "C:/Users/ulver/.vcpkg-clion/vcpkg/packages/tbb_x64-windows")
find_package(pxr CONFIG REQUIRED HINTS "C:/Users/ulver/.vcpkg-clion/vcpkg/packages/usd_x64-windows")

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

add_library("imgui" STATIC ${IMGUI_GLOB})
target_include_directories("imgui" PUBLIC ${IMGUI_PATH})
target_link_libraries("imgui" PRIVATE glfw Vulkan::Vulkan)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

target_link_libraries(
        ${PROJECT_NAME} PRIVATE
        Vulkan::Vulkan
        glfw
        fastgltf::fastgltf
        imgui
        KTX::ktx
        meshoptimizer::meshoptimizer
        ar gf js tf
        TBB::tbb TBB::tbbmalloc TBB::tbbmalloc_proxy
)

if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
    set(SLANGC "$ENV{VULKAN_SDK}/Bin/slangc.exe")
else()
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
    set(SLANGC "$ENV{VULKAN_SDK}/Bin32/slangc.exe")
endif()

file(GLOB_RECURSE GLSL_SOURCE_FILES
        "${PROJECT_SOURCE_DIR}/shaders/src/glsl/*.frag"
        "${PROJECT_SOURCE_DIR}/shaders/src/glsl/*.vert"
        "${PROJECT_SOURCE_DIR}/shaders/src/glsl/*.comp"
)

file (GLOB SLANG_SOURCE_FILES "${PROJECT_SOURCE_DIR}/shaders/src/slang/*.slang")
file (GLOB SLANG_MODULE_FILES "${PROJECT_SOURCE_DIR}/shaders/src/slang/modules/*.slang")

foreach(GLSL ${GLSL_SOURCE_FILES})
    message(STATUS "BUILDING GLSL SHADER")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/bin/glsl/${FILE_NAME}.spv")
    message(STATUS ${GLSL})
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/bin/glsl"
            COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV} -g
            DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

foreach (SLANG_MODULE ${SLANG_MODULE_FILES})
    message(STATUS "BUILDING SLANG MODULE")
    get_filename_component(FILE_NAME ${SLANG_MODULE} NAME_WLE)
    set(MODULE "${PROJECT_SOURCE_DIR}/shaders/src/slang/${FILE_NAME}.slang-module")
    message(STATUS ${SLANG_MODULE})
    add_custom_command(
            OUTPUT ${MODULE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/src/slang -fvk-use-scalar-layout"
            COMMAND ${SLANGC} -o ${MODULE} ${SLANG_MODULE})
    list(APPEND SPIRV_BINARY_FILES ${MODULE})
endforeach (SLANG_MODULE)

foreach (SLANG ${SLANG_SOURCE_FILES})
    message(STATUS "BUILDING SLANG SHADER")
    get_filename_component(FILE_NAME ${SLANG} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/bin/slang/${FILE_NAME}.spv")
    message(STATUS ${SLANG})
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/bin/slang -fvk-use-scalar-layout"
            COMMAND ${SLANGC} -target spirv ${SLANG} -o ${SPIRV} -g)
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach (SLANG)

add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/shaders/bin/glsl"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/shaders/bin/slang"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/shaders/src/glsl/"
        "${PROJECT_SOURCE_DIR}/shaders/src/glsl/"

        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/shaders/src/slang/"
        "${PROJECT_SOURCE_DIR}/shaders/src/slang/"
)