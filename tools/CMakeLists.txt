file(GLOB files "*.c")

foreach(file ${files})
    get_filename_component(name ${file} NAME_WLE)
    add_executable(${name} ${file})
    target_include_directories(${name} PRIVATE ${LIB_INCLUDES})
    target_link_libraries(${name} ${LIB_NAME})
    target_compile_options(${name} PRIVATE ${LIB_C_FLAGS})
endforeach()
