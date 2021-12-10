project(fastcut)

add_executable(fastcut src/main.c src/decoder.c src/encoder.c src/fast_cut.c src/preprocess.c src/reencoding.c src/stream_ctx.c src/utils.c)
target_link_libraries(fastcut avformat avutil avcodec)
