option(
    AUTOPAS_USE_VECTORIZATION "Enable generations of SIMD vector instructions through omp-simd" ON
)
if (AUTOPAS_USE_VECTORIZATION)
    message(STATUS "Vectorization enabled.")
    # list of available options
    set(VECTOR_INSTRUCTIONS_OPTIONS "NATIVE;SSE;AVX;AVX2;KNL")
    # set instruction set type
    set(
        AUTOPAS_VECTOR_INSTRUCTIONS
        "NATIVE"
        CACHE STRING "Vector instruction set to use (${VECTOR_INSTRUCTIONS_OPTIONS})."
    )
    # let ccmake and cmake-gui offer the options
    set_property(CACHE AUTOPAS_VECTOR_INSTRUCTIONS PROPERTY STRINGS ${VECTOR_INSTRUCTIONS_OPTIONS})

    if (AUTOPAS_ENABLE_CUDA)
        target_compile_options(
            autopas
            PUBLIC
                # openmp simd
                $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=>-fopenmp-simd>
                $<$<CXX_COMPILER_ID:Intel>:$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=>-qopenmp-simd>
                # vector instruction set
                $<$<STREQUAL:${AUTOPAS_VECTOR_INSTRUCTIONS},NATIVE>:$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=>-march=native>
                $<$<STREQUAL:${AUTOPAS_VECTOR_INSTRUCTIONS},SSE>:$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=>-msse3>
                $<$<STREQUAL:${AUTOPAS_VECTOR_INSTRUCTIONS},AVX>:$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=>-mavx>
                $<$<AND:$<STREQUAL:${AUTOPAS_VECTOR_INSTRUCTIONS},AVX2>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=>-mavx2
                $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=>-mfma>
                $<$<AND:$<STREQUAL:${AUTOPAS_VECTOR_INSTRUCTIONS},AVX2>,$<CXX_COMPILER_ID:Intel>>:$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=>-march=core-avx2
                $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=>-fma>
                $<$<AND:$<STREQUAL:${AUTOPAS_VECTOR_INSTRUCTIONS},KNL>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=>-march=knl>
                $<$<AND:$<STREQUAL:${AUTOPAS_VECTOR_INSTRUCTIONS},KNL>,$<CXX_COMPILER_ID:Intel>>:$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=>-xMIC-AVX512>
        )
    else ()
        # the else branch can be removed once we enforce cmake >=3.12 see:
        # https://gitlab.kitware.com/cmake/cmake/issues/17952
        target_compile_options(
            autopas
            PUBLIC
                # openmp simd
                $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-fopenmp-simd>
                $<$<CXX_COMPILER_ID:Intel>:-qopenmp-simd>
                # vector instruction set
                $<$<STREQUAL:${AUTOPAS_VECTOR_INSTRUCTIONS},NATIVE>:-march=native>
                $<$<STREQUAL:${AUTOPAS_VECTOR_INSTRUCTIONS},SSE>:-msse3>
                $<$<STREQUAL:${AUTOPAS_VECTOR_INSTRUCTIONS},AVX>:-mavx>
                $<$<AND:$<STREQUAL:${AUTOPAS_VECTOR_INSTRUCTIONS},AVX2>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:-mavx2
                -mfma>
                $<$<AND:$<STREQUAL:${AUTOPAS_VECTOR_INSTRUCTIONS},AVX2>,$<CXX_COMPILER_ID:Intel>>:-march=core-avx2
                -fma>
                $<$<AND:$<STREQUAL:${AUTOPAS_VECTOR_INSTRUCTIONS},KNL>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:-march=knl>
                $<$<AND:$<STREQUAL:${AUTOPAS_VECTOR_INSTRUCTIONS},KNL>,$<CXX_COMPILER_ID:Intel>>:-xMIC-AVX512>
        )
    endif ()

else ()
    message(STATUS "Vectorization disabled.")

    if (AUTOPAS_ENABLE_CUDA)
        target_compile_options(
            autopas
            PUBLIC
                $<$<CXX_COMPILER_ID:GNU>:$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=>-fno-tree-vectorize>
                $<$<CXX_COMPILER_ID:Clang>:$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=>-fno-vectorize>
                $<$<CXX_COMPILER_ID:Intel>:$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=>-no-vec>
        )
    else ()
        target_compile_options(
            autopas
            PUBLIC
                $<$<CXX_COMPILER_ID:GNU>:-fno-tree-vectorize>
                $<$<CXX_COMPILER_ID:Clang>:-fno-vectorize> $<$<CXX_COMPILER_ID:Intel>:-no-vec>
        )
    endif ()
endif ()
