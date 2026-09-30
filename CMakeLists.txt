cmake_minimum_required(VERSION 3.25)

project(DiskLens VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup()

include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
endif()

file(GLOB_RECURSE DISKLENS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.ui"
)

qt_add_executable(DiskLens WIN32 ${DISKLENS_SOURCES})

target_link_libraries(DiskLens PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_compile_definitions(DiskLens PRIVATE
    DISKLENS_VERSION=\"${PROJECT_VERSION}\"
)

set(DISKLENS_QT_DEPLOY_TOOL "" CACHE FILEPATH "Path to windeployqt.exe")

if(WIN32 AND DISKLENS_QT_DEPLOY_TOOL)
    add_custom_command(TARGET DiskLens POST_BUILD
        COMMAND "${DISKLENS_QT_DEPLOY_TOOL}"
            --no-translations
            "$<TARGET_FILE:DiskLens>"
        COMMENT "Deploying Qt runtime files for DiskLens"
        VERBATIM
    )
endif()

if(BUILD_TESTING)
    qt_add_executable(DuplicateFinderTests
        tests/DuplicateFinderTests.cpp
        src/core/analysis/DuplicateFinder.cpp
    )

    target_link_libraries(DuplicateFinderTests PRIVATE
        Qt6::Core
        Qt6::Test
    )

    add_test(NAME DuplicateFinderTests COMMAND DuplicateFinderTests)

    if(WIN32)
        set_tests_properties(DuplicateFinderTests PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
    endif()
endif()
