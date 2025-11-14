macro(subdirlist result curdir)
    file(GLOB children RELATIVE ${curdir} ${curdir}/*)
    set(dirlist "")
    foreach(child ${children})
        if(IS_DIRECTORY ${curdir}/${child})
            list(APPEND dirlist ${child})
        endif()
    endforeach()
    set(${result} ${dirlist})
endmacro()

subdirlist(subdirs ${CMAKE_CURRENT_SOURCE_DIR})

foreach(name ${subdirs})

    add_executable(${name} ${name}/${name}.c)
    target_include_directories(${name} PRIVATE ${LIB_INCLUDES})
    target_link_libraries(${name} ${LIB_NAME})
    target_compile_options(${name} PRIVATE ${LIB_C_FLAGS})

endforeach()
