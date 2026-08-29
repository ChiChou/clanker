cmake_minimum_required(VERSION 3.10)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" QR_VERSION)
string(STRIP "${QR_VERSION}" QR_VERSION)

project(qr VERSION "${QR_VERSION}" LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(qr qr.c qrcodegen.c)
target_compile_definitions(qr PRIVATE QR_VERSION="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(qr PRIVATE /W4 /wd4100)
else()
    target_compile_options(qr PRIVATE -Wall -Wextra)
endif()

install(TARGETS qr RUNTIME DESTINATION bin)

enable_testing()
add_test(NAME qr-version COMMAND qr --version)
set_tests_properties(qr-version PROPERTIES PASS_REGULAR_EXPRESSION "qr ${PROJECT_VERSION}")
