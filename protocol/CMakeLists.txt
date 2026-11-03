add_library(focp STATIC
    src/focp.c
    src/focp_frame.c
    src/focp_dev.c
)
target_include_directories(focp PUBLIC include)
set_target_properties(focp PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(focp PRIVATE -Wall -Wextra -Wconversion -Wno-sign-conversion -pedantic)
endif()
