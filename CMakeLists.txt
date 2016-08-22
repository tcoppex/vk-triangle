cmake_minimum_required(VERSION 2.8.12)

project(hello_vk_triangle)

# Paths
# -----

# cmake extensions path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# project paths
set(SRC_DIR         "${CMAKE_SOURCE_DIR}/src")
set(THIRDPARTY_DIR  "${CMAKE_SOURCE_DIR}/thirdparty")
set(SHADERS_DIR     "${CMAKE_SOURCE_DIR}/shaders")
#set(SHADERS_BIN_DIR "${SHADERS_DIR}/binary")


# Dependencies
# ------------

find_package(XCB REQUIRED)

# TODO : find_package(Vulkan REQUIRED)
set(VULKAN_INCLUDE_DIRS ${VK_INCLUDE_DIRS} "$ENV{VULKAN_SDK}/include")
set(VULKAN_LIBRARIES "vulkan")
link_directories($ENV{VULKAN_SDK}/lib)

# TODO : find_package(VulkanSDK 1.0 REQUIRED)
find_program(GLSL_LANG_VALIDATOR glslangValidator)

# macro : convert a GLSL shader to its SPIRV version
macro(_convert_glsl_to_spirv input_glsl output_spirv)  
  add_custom_command(
    OUTPUT
      ${output_spirv}
    COMMAND
      ${GLSL_LANG_VALIDATOR} -s -V -o ${output_spirv} ${input_glsl}
    DEPENDS
      ${input_glsl}
      ${GLSL_LANG_VALIDATOR}
    WORKING_DIRECTORY
      ${CMAKE_SOURCE_DIR}
    COMMENT
      "Converting shader ${input_glsl} to ${output_spirv}" VERBATIM
    SOURCES
      ${input_glsl}
  )
endmacro(_convert_glsl_to_spirv)

add_definitions(
  -DEXTERNAL_SPV
  -DVK_PROTOTYPES
  -DVK_USE_PLATFORM_XCB_KHR
)


# Main target build
# -----------------

# TODO : add_subdirectory(${SRC_DIR})

set(TARGET_NAME vk_triangle)

# TODO : do not use glob for sources, prefers to set files manually
file(GLOB_RECURSE Sources ${SRC_DIR}/*.cc)
file(GLOB_RECURSE Headers ${SRC_DIR}/*.h)
file(GLOB ShadersGLSL     ${SHADERS_DIR}/*.vert ${SHADERS_DIR}/*.tesc
                          ${SHADERS_DIR}/*.tese ${SHADERS_DIR}/*.geom
                          ${SHADERS_DIR}/*.frag ${SHADERS_DIR}/*.comp)

# Convert each raw GLSL shaders into a SpirV binary
foreach(glslshader IN LISTS ShadersGLSL)
  set(shader_binary_name ${glslshader}.spv)
  _convert_glsl_to_spirv(${glslshader} ${shader_binary_name})
  list(APPEND ShadersSPIRV  ${shader_binary_name})
endforeach()

add_executable(${TARGET_NAME}
  ${Sources}
  ${Headers}
  ${ShadersSPIRV}
)

if(CMAKE_COMPILER_IS_GNUCXX)
  set(CXX_FLAGS         "-std=c++11 -Wall -Wno-unused-function")
  set(CXX_FLAGS_DEBUG   "-Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
  set(CXX_FLAGS_RELEASE "-03 -DNDEBUG")

  add_definitions(-DSHADERS_DIR="${SHADERS_DIR}/")
else()
  message(WARNING "This compiler has not been tested.")
endif()

set_target_properties(${TARGET_NAME} PROPERTIES
  COMPILE_FLAGS "${CXX_FLAGS}"
)

include_directories(
  ${SRC_DIR}
  ${THIRDPARTY_DIR}
  ${XCB_INCLUDE_DIRS}
  ${VULKAN_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
  ${XCB_LIBRARIES}
  ${VULKAN_LIBRARIES}
)
