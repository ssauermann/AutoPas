file(
    GLOB_RECURSE
    MDFlex_SRC
    "../*.cpp"
    "../*.h"
)

# remove main
foreach (FILE_PATH ${MDFlex_SRC})
    string(FIND ${FILE_PATH} "main.cpp" EXCLUDE_FILE)
    if (NOT ${EXCLUDE_FILE} EQUAL -1)
        list(REMOVE_ITEM MDFlex_SRC ${FILE_PATH})
    endif ()
endforeach ()

add_executable(mdFlexTests ${MDFlex_SRC})

target_compile_definitions(
    mdFlexTests PRIVATE
    YAMLDIRECTORY=\"${PROJECT_SOURCE_DIR}/examples/md-flexible/tests/yamlTestFiles/\"
)
target_include_directories(
    mdFlexTests
    PUBLIC ${PROJECT_SOURCE_DIR}/tests/testAutopas ${PROJECT_SOURCE_DIR}/examples/md-flexible
)

target_link_libraries(
    mdFlexTests
    PUBLIC
        autopas
        autopasTools
        libgtest
        libgmock
        yaml-cpp
)

# this cmake module was only introduced in 3.10
include(GoogleTest)
# more robust, queries the compiled executable
gtest_discover_tests(mdFlexTests TEST_PREFIX "mdFlexTests/")
