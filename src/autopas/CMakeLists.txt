file(
    GLOB_RECURSE
    MY_SRC
    "*.cpp"
    "*.h"
)

file(
    GLOB_RECURSE
    CU_SRC
    "*.cu"
    "*.cuh"
)

add_library(autopas STATIC $<$<BOOL:${AUTOPAS_ENABLE_CUDA}>:${CU_SRC}> ${MY_SRC})

target_link_libraries(
    autopas
    PUBLIC
        rt # required for Time.h
        ${CMAKE_THREAD_LIBS_INIT} # required for Logger and ExceptionHandler Do not link against
                                  # openmp when using archer
        $<$<AND:$<BOOL:${AUTOPAS_OPENMP}>,$<NOT:$<BOOL:${ARCHER}>>>:OpenMP::OpenMP_CXX>
        Eigen3
        spdlog::spdlog
        harmony
)

# Ompstuff needs to be here because OpenMP.cmake needs to run before this file to create to OpenMP
# target. this can be resolved by upgrading to CMake 3.13 and enforcing CMP0079.
target_compile_definitions(
    autopas
    PUBLIC
    $<$<BOOL:${AUTOPAS_OPENMP}>:AUTOPAS_OPENMP>
    $<$<BOOL:${AUTOPAS_ENABLE_CUDA}>:AUTOPAS_CUDA>
    $<$<NOT:$<BOOL:${AUTOPAS_OPENMP}>>:EIGEN_DONT_PARALLELIZE>
)

if (AUTOPAS_ENABLE_CUDA)
    target_include_directories(autopas SYSTEM PUBLIC ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})
endif ()

target_include_directories(autopas PUBLIC ../.)
