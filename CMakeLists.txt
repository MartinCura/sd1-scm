cmake_minimum_required(VERSION 3.6)
project(sd1_scm)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(DEPENDENCIES
        common/constants.h
        common/message.h
        common/log/log.h
        common/log/log.c
        common/ipc/resources.h
        common/ipc/sig.h
        common/ipc/sig.c
        common/ipc/socket.h
        common/ipc/socket.c
        common/ipc/msg_queue.h
        common/ipc/msg_queue.c
        common/ipc/semaphore.h
        common/ipc/semaphore.c
        common/ipc/shm.h
        common/ipc/shm.c)

add_executable(scm-client    client/main.cpp        ${DEPENDENCIES})
add_executable(local-broker  client/broker.cpp      ${DEPENDENCIES})
add_executable(scm-server    server/server.cpp      ${DEPENDENCIES})
add_executable(server-worker server/worker.cpp      ${DEPENDENCIES})
add_executable(ring-node     server/ring_node.cpp   ${DEPENDENCIES})
