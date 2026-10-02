file(GLOB_RECURSE files "*.c")

add_library(Fusion ${files})

target_include_directories(Fusion PUBLIC .)

# The target MCU has 64 KiB of flash.  Keep the library size-efficient even in
# application Debug builds while retaining generated debug information.
target_compile_options(Fusion PRIVATE -Os)

# Fusion uses sinf, cosf, powf, and other functions from libm.  Declaring the
# dependency on the library target keeps libm after libFusion in the embedded
# linker command, which is required for static-library symbol resolution.
target_link_libraries(Fusion PUBLIC m)
