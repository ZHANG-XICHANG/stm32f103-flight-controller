cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME P01_flight_hal)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/com_debug.c
    Core/Inc/com_debug.h
    Core/Src/nRF24L01P.c
    Core/Inc/nRF24L01P.h
    Core/Inc/nRF24L01P_REG.h
    Core/Src/ESC.c
    Core/Inc/ESC.h
    Core/Src/flight_state.c
    Core/Inc/flight_state.h
    Core/Src/key.c
    Core/Inc/key.h
    Core/Src/receive.c
    Core/Inc/receive.h
    Core/Src/MPU6050.c
    Core/Inc/MPU6050.h
    Core/Src/flight.c
    Core/Inc/flight.h
    Core/Src/com_filter.c
    Core/Inc/com_filter.h
    Core/Src/com_pid.c
    Core/Inc/com_pid.h
    Core/Src/pid_command.c
    Core/Src/telemetry.c
    Core/Inc/telemetry.h
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)
add_subdirectory(Middlewares/Third_Party/Fusion)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    Fusion

    # Add user defined libraries
)
