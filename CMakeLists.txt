cmake_minimum_required(VERSION 3.20)
project(chubby_cat CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT APPLE)
    message(FATAL_ERROR "chubby-cat targets macOS (uses Hypervisor.framework).")
endif()

add_executable(chubby-cat
    src/main.cpp
    src/vm.cpp
    src/vcpu.cpp
)

target_include_directories(chubby-cat PRIVATE src)
target_compile_options(chubby-cat PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(chubby-cat PRIVATE "-framework Hypervisor")

# Hypervisor.framework requires the binary to be code-signed with the
# com.apple.security.hypervisor entitlement. We do an ad-hoc signature
# at build time so the binary is runnable from the build directory.
set(ENTITLEMENTS_FILE "${CMAKE_SOURCE_DIR}/chubby-cat.entitlements")
add_custom_command(TARGET chubby-cat POST_BUILD
    COMMAND codesign --sign - --entitlements "${ENTITLEMENTS_FILE}"
            --force "$<TARGET_FILE:chubby-cat>"
    COMMENT "codesign: ad-hoc signing chubby-cat with hypervisor entitlement"
    VERBATIM
)
