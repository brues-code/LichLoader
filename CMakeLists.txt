cmake_minimum_required(VERSION 3.16...3.26)

# Default to Release if no build type was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to 'Release' as none was specified.")
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
else()
	message(STATUS "Build type is set to '${CMAKE_BUILD_TYPE}'.")
endif()

# Force Win32 for the Visual Studio generator (must happen before project())
if(CMAKE_GENERATOR MATCHES "Visual Studio")
	set(CMAKE_GENERATOR_PLATFORM Win32)
endif()

project(lichloader LANGUAGES C)

# WoW 3.3.5a is 32-bit; CreateRemoteThread(LoadLibraryW) requires the launcher
# to share bitness with the target, so a 64-bit build cannot work.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(
		FATAL_ERROR
		"A 32-bit environment is required to compile LichLoader. Launch CMake with the x86 VS Native Tools command prompt."
	)
endif()

add_subdirectory(src/launcher)
