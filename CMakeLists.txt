cmake_minimum_required(VERSION 3.5)

project(tcp-server-demo LANGUAGES C)

add_executable(tcp-server-demo
    main.c
    hashmap.c
    pthreadpool.c
    tcpserver.c
)

target_link_libraries(tcp-server-demo
    pthread
)
