cmake_minimum_required(VERSION 3.0)

project(OmniCache)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(INC
	.
	intern
)

set(SRC
	intern/omnicache.c
	intern/utils.c
	intern/omni_utils.c
)

include_directories(${INC})

if (MSVC)
	set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} /W4")
else()
	set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -Wstrict-prototypes -Wmissing-prototypes -Wlogical-op -Winit-self -Wshadow -Wcast-qual")
endif()

add_library(omnicache SHARED ${SRC})

set_target_properties(omnicache PROPERTIES PUBLIC_HEADER "omnicache.h;intern/types.h")

install(TARGETS omnicache
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include/omnicache)
