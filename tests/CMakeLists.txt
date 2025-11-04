file(GLOB files "*.c")

foreach(file ${files})
    get_filename_component(name ${file} NAME_WLE)
    add_executable(${name} ${file} ${COMMON_SOURCES})
    target_include_directories(${name} PRIVATE ${COMMON_INCLUDES})
    target_link_libraries(${name} ${COMMON_LIBS})
    message(${COMMON_C_FLAGS})
    target_compile_options(${name} PRIVATE ${COMMON_C_FLAGS})

    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endforeach()
