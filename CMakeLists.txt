# requiered CMAKE Version
cmake_minimum_required(VERSION 3.8)

# name of current project
project("PathTracer")

# add compiler flags
set(CMAKE_CXX_STANDARD 17)
add_compile_options(-mf16c -mfma -mavx2)

#add include directories
include_directories("C:/VulkanSDK/1.2.170.0/Include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/lib/vka-1.3.1")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/lib/stb_master")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/lib/tiny_obj_loader")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/lib/glm2")

# add link directories
link_directories("C:/VulkanSDK/1.2.170.0/Lib")

# create libraries
add_library(Setup_lib
    "src/Setup/init.cpp"
    "src/Setup/instance.cpp"
    "src/Setup/device.cpp"
    "src/Setup/queue.cpp"
)

add_library(PathTracer_lib 
    "src/PathTracer/init.cpp"
    "src/PathTracer/pool.cpp"
    "src/PathTracer/images.cpp"
    "src/PathTracer/model.cpp"
    "src/PathTracer/material.cpp"
    "src/PathTracer/acceleration_structure.cpp"
    "src/PathTracer/descriptor.cpp"
    "src/PathTracer/pipeline.cpp"
    "src/PathTracer/sbt.cpp"
    "src/PathTracer/raytrace.cpp"
)
# add_library(LiveImage_lib )

# crate executable
add_executable(PathTracer "main.cpp")

# link external libraries
target_link_libraries(PathTracer PUBLIC
    "-lvulkan-1"
)     

# link internal libraries
target_link_libraries(PathTracer PUBLIC
    Setup_lib
    PathTracer_lib
)

# additional work
set(CMAKE_EXPORT_COMPILE_COMMANDS on)
