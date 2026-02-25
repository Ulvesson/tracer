# CMakeList.txt : CMake project for tracer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("tracer")

# Tracer as a reusable library (header-only since all logic is in tracer.h)
add_library (tracer_lib INTERFACE)
target_include_directories (tracer_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features (tracer_lib INTERFACE cxx_std_20)

# Option to completely disable tracing in release builds
option (TRACER_ENABLED "Enable trace instrumentation" ON)
if (NOT TRACER_ENABLED)
  target_compile_definitions (tracer_lib INTERFACE TRACER_DISABLED)
endif()

# Demo executable
add_executable (tracer "test.cpp" "tracer.cpp" "tracer.h")
target_link_libraries (tracer PRIVATE tracer_lib)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET tracer PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
