# CMakeList.txt: VideoCorruptor 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("VideoCorruptor")

# 将源代码添加到此项目的可执行文件。
SET (PROJECT_FILES
	"MP4Corruptor.cpp"
	"MP4Corruptor.h"
	"main.cpp"
	"AVICorruptor.cpp"
	"AVICorruptor.h"
	"VideoCorruptor.h"
)
add_executable (VideoCorruptor ${PROJECT_FILES})

#specify utf-8 encoding for windows
if (MSVC)
	target_compile_options(VideoCorruptor PRIVATE /source-charset:utf-8 /execution-charset:utf-8)
else()
    target_compile_options(VideoCorruptor PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()




if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET VideoCorruptor PROPERTY CXX_STANDARD 17)
endif()

# TODO: 如有需要，请添加测试并安装目标。
