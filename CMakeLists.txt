# 机器视觉课程作业 —— 实验 01：RGB 图像像素操作
# 环境：Windows 10/11 + Visual Studio 2022 (MSVC x64) + OpenCV 5.0.0 + CMake 3.16+

cmake_minimum_required(VERSION 3.16)

project(machine_vision_homework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源码含中文注释，MSVC 需要按 UTF-8 解析
if(MSVC)
    add_compile_options(/utf-8)
endif()

# OpenCV 的默认安装位置（换机器时可用 -DOpenCV_DIR=... 覆盖）
if(NOT OpenCV_DIR)
    set(OpenCV_DIR "D:/OpenCV_5.0.0/opencv/build" CACHE PATH "OpenCV build 目录")
endif()

find_package(OpenCV REQUIRED)

message(STATUS "OpenCV 版本: ${OpenCV_VERSION}")
message(STATUS "OpenCV 库目录: ${OpenCV_DIR}")

add_executable(rgb_pixel_demo src/main.cpp)

target_include_directories(rgb_pixel_demo PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(rgb_pixel_demo PRIVATE ${OpenCV_LIBS})

# 把 OpenCV 的 dll 复制到可执行文件旁边，否则双击 exe 会提示找不到 opencv_world500.dll
if(WIN32)
    file(GLOB OpenCV_DLLS
        "${OpenCV_DIR}/x64/vc16/bin/opencv_world*.dll"
        "${OpenCV_DIR}/x64/vc17/bin/opencv_world*.dll"
    )
    if(OpenCV_DLLS)
        add_custom_command(TARGET rgb_pixel_demo POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OpenCV_DLLS} $<TARGET_FILE_DIR:rgb_pixel_demo>
            COMMENT "复制 OpenCV 运行时 dll"
        )
    endif()
endif()
