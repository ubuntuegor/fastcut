project(fastcut)

add_executable(main src/main.c)
target_link_libraries(main avformat avutil avcodec)
