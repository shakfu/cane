cmake_minimum_required(VERSION 3.25)  # CXX_STANDARD 26

# License: AGPL-3.0-only
project(
	cane
	VERSION 1.0
	DESCRIPTION "MIDI Sequencing Language"
	LANGUAGES C CXX
)

option(CANE_SANITIZERS "enable sanitizers" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Native backends only: CoreMIDI, ALSA, JACK, WinMM. The disabled ones
# download dependencies at configure time (PipeWire, WinMIDI, WinUWP)
# or need Boost (network).
set(LIBREMIDI_NO_BOOST ON)
set(LIBREMIDI_NO_NETWORK ON)
set(LIBREMIDI_NO_KEYBOARD ON)
set(LIBREMIDI_NO_PIPEWIRE ON)
set(LIBREMIDI_NO_WINMIDI ON)
set(LIBREMIDI_NO_WINUWP ON)
set(LIBREMIDI_DOWNLOAD_CPPWINRT OFF)

# Static, so the installed cane needs no libremidi. EXCLUDE_FROM_ALL also
# keeps libremidi's install rules out of `cmake --install`.
# Before add_compile_options so cane's warning flags do not apply to them.
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(thirdparty/libremidi-5.4.3 EXCLUDE_FROM_ALL SYSTEM)

add_library(linenoise STATIC
	thirdparty/linenoise/src/linenoise.c
	thirdparty/linenoise/src/internal/utf8.c
)
target_include_directories(linenoise SYSTEM PUBLIC thirdparty/linenoise/include)
target_include_directories(linenoise PRIVATE thirdparty/linenoise/include/internal)

if(MSVC)
	add_compile_options(/W4)
else()
	add_compile_options(-Wall -Wextra -Wno-unused-function)
endif()

# MSVC supports only the address sanitizer.
if(CANE_SANITIZERS AND MSVC)
	add_compile_options(/fsanitize=address)
elseif(CANE_SANITIZERS)
	add_compile_options(-fsanitize=address,undefined)
	add_link_options(-fsanitize=address,undefined)
endif()

add_executable(cane src/cane.cpp)
target_include_directories(cane PRIVATE include)
find_package(Threads REQUIRED)

target_link_libraries(cane PRIVATE libremidi::libremidi linenoise Threads::Threads)

install(TARGETS cane)

# Unit tests
if(PROJECT_IS_TOP_LEVEL)
	enable_testing()

	set(CANE_TESTS_PASS pass midi watch eval cli player)
	set(CANE_TESTS_FAIL fail die assert)

	foreach(name IN LISTS CANE_TESTS_PASS CANE_TESTS_FAIL)
		add_executable(${name} tests/${name}.cpp)
		target_include_directories(${name} PRIVATE include)
		target_link_libraries(${name} PRIVATE libremidi::libremidi Threads::Threads)
		add_test(NAME ${name} COMMAND ${name})
	endforeach()

	set_tests_properties(${CANE_TESTS_FAIL} PROPERTIES WILL_FAIL TRUE)

	add_executable(readme tests/readme.cpp)
	target_include_directories(readme PRIVATE include)
	target_link_libraries(readme PRIVATE Threads::Threads)
	add_test(NAME readme COMMAND readme ${PROJECT_SOURCE_DIR}/README.md)

	add_test(
		NAME repl
		COMMAND ${CMAKE_COMMAND}
			-DCANE=$<TARGET_FILE:cane>
			-DINPUT=${PROJECT_SOURCE_DIR}/tests/repl.txt
			-P ${PROJECT_SOURCE_DIR}/tests/repl.cmake
	)

	# Threaded tests (player, watch) fail by hanging; make that a failure.
	set_tests_properties(
		${CANE_TESTS_PASS} ${CANE_TESTS_FAIL} readme repl PROPERTIES TIMEOUT 30
	)
endif()
