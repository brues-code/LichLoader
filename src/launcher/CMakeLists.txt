add_executable(lichloader_launcher WIN32
	"main.c"
	"cmdline.c"

	"${CMAKE_SOURCE_DIR}/src/loader.c"
	"${CMAKE_SOURCE_DIR}/src/os.c"
	"${CMAKE_SOURCE_DIR}/src/textfile.c"

	"${CMAKE_SOURCE_DIR}/res/lichloader.rc"
)

target_compile_features(lichloader_launcher PRIVATE c_std_99)

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
	target_compile_options(lichloader_launcher PRIVATE "/W3")
	set_target_properties(lichloader_launcher PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded")
	target_link_options(lichloader_launcher PRIVATE "/MANIFEST:NO")
else()
	target_compile_options(lichloader_launcher PRIVATE "-Wall" "-Wno-unused-variable" "-municode" "-fno-strict-aliasing")
	target_link_options(lichloader_launcher PRIVATE "-static-libgcc" "-municode")

	if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
		target_link_options(lichloader_launcher PRIVATE "-s")
	endif()
endif()

target_compile_definitions(lichloader_launcher PRIVATE -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS)
target_compile_definitions(lichloader_launcher PRIVATE
	LL_VER_MAJOR=${LICHLOADER_VER_MAJOR}
	LL_VER_MINOR=${LICHLOADER_VER_MINOR}
	LL_VER_PATCH=${LICHLOADER_VER_PATCH}
	LL_VER_BUILD=${LICHLOADER_VER_BUILD}
)
target_include_directories(lichloader_launcher PRIVATE "${CMAKE_SOURCE_DIR}/src")

target_link_libraries(lichloader_launcher shlwapi)

set_target_properties(lichloader_launcher PROPERTIES OUTPUT_NAME "LichLoader")
set_target_properties(lichloader_launcher PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
