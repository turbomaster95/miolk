cmake_minimum_required(VERSION 4.1)

project(miolk C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wno-unused)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

set(M_SRCS
	miolk.c

)

add_library(miolk_s STATIC ${M_SRCS})
set_target_properties(miolk_s PROPERTIES OUTPUT_NAME miolk)
set_target_properties(miolk_s PROPERTIES
            POSITION_INDEPENDENT_CODE ON
)
