add_library(lichcore SHARED
	"dll.c"

	"${CMAKE_SOURCE_DIR}/src/loader.c"
	"${CMAKE_SOURCE_DIR}/src/textfile.c"
)

target_compile_features(lichcore PRIVATE c_std_99)

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
	target_compile_options(lichcore PRIVATE "/W3")
	set_target_properties(lichcore PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded")
	target_link_options(lichcore PRIVATE "/MANIFEST:NO")
else()
	target_compile_options(lichcore PRIVATE "-Wall" "-Wno-unused-variable" "-municode" "-fno-strict-aliasing")
	target_link_options(lichcore PRIVATE "-static-libgcc" "-municode")

	if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
		target_link_options(lichcore PRIVATE "-s")
	endif()
endif()

target_compile_definitions(lichcore PRIVATE -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS)
target_include_directories(lichcore PRIVATE "${CMAKE_SOURCE_DIR}/src")

# MinHook compiled directly into the coordinator (32-bit disassembler only)
target_include_directories(lichcore PRIVATE "${CMAKE_SOURCE_DIR}/external/minhook/include")
target_sources(lichcore PRIVATE
	"${CMAKE_SOURCE_DIR}/external/minhook/src/buffer.c"
	"${CMAKE_SOURCE_DIR}/external/minhook/src/hook.c"
	"${CMAKE_SOURCE_DIR}/external/minhook/src/trampoline.c"
	"${CMAKE_SOURCE_DIR}/external/minhook/src/hde/hde32.c"
)

target_link_libraries(lichcore shlwapi)

set_target_properties(lichcore PROPERTIES PREFIX "")
set_target_properties(lichcore PROPERTIES OUTPUT_NAME "LichCore")
# Trailing $<0:> suppresses the per-config subdir multi-config generators append.
set_target_properties(lichcore PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<0:>")
