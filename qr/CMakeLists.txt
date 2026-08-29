cmake_minimum_required(VERSION 3.10)
project(qr C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(qr qr.c qrcodegen.c)

if(MSVC)
    target_compile_options(qr PRIVATE /W4 /wd4100)
else()
    target_compile_options(qr PRIVATE -Wall -Wextra)
endif()

install(TARGETS qr RUNTIME DESTINATION bin)
