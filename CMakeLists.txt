cmake_minimum_required(VERSION 2.6)

project(enet)

# The "configure" step.
include(CheckFunctionExists)
include(CheckCSourceCompiles)
include(CheckTypeSize)
check_function_exists("fcntl" HAS_FCNTL)
check_function_exists("poll" HAS_POLL)
check_function_exists("getaddrinfo" HAS_GETADDRINFO)
check_function_exists("getnameinfo" HAS_GETNAMEINFO)
check_function_exists("gethostbyname_r" HAS_GETHOSTBYNAME_R)
check_function_exists("gethostbyaddr_r" HAS_GETHOSTBYADDR_R)
check_function_exists("inet_pton" HAS_INET_PTON)
check_function_exists("inet_ntop" HAS_INET_NTOP)

# Would like to use check_struct_has_memeber, but the clang static analyzer tries to compile
# that and complains. So we want to include #ifndef __clang_analyzer__ in the generated
# source.
check_c_source_compiles(
   "#ifndef __clang_analyzer__
    #include <sys/types.h>
    #include <sys/socket.h>

    int main()
    {
        struct msghdr* tmp;
        (void)tmp->msg_flags;
        return 0;
    }
    #endif" HAS_MSGHDR_FLAGS)

set(CMAKE_EXTRA_INCLUDE_FILES "sys/types.h" "sys/socket.h")
check_type_size("socklen_t" HAS_SOCKLEN_T BUILTIN_TYPES_ONLY)
unset(CMAKE_EXTRA_INCLUDE_FILES)

set (TARGET_NAME
    enet
)

if(HAS_FCNTL)
    add_definitions(-DHAS_FCNTL=1)
endif()
if(HAS_POLL)
    add_definitions(-DHAS_POLL=1)
endif()
if(HAS_GETNAMEINFO)
    add_definitions(-DHAS_GETNAMEINFO=1)
endif()
if(HAS_GETADDRINFO)
    add_definitions(-DHAS_GETADDRINFO=1)
endif()
if(HAS_GETHOSTBYNAME_R)
    add_definitions(-DHAS_GETHOSTBYNAME_R=1)
endif()
if(HAS_GETHOSTBYADDR_R)
    add_definitions(-DHAS_GETHOSTBYADDR_R=1)
endif()
if(HAS_INET_PTON)
    add_definitions(-DHAS_INET_PTON=1)
endif()
if(HAS_INET_NTOP)
    add_definitions(-DHAS_INET_NTOP=1)
endif()
if(HAS_MSGHDR_FLAGS)
    add_definitions(-DHAS_MSGHDR_FLAGS=1)
endif()
if(HAS_SOCKLEN_T)
    add_definitions(-DHAS_SOCKLEN_T=1)
endif()

include_directories(${PROJECT_SOURCE_DIR}/include)
set (SOURCES
        callbacks.c
        compress.c
        host.c
        list.c
        packet.c
        peer.c
        protocol.c
    )

if (WIN32)
    add_definitions(-DENET_DLL=1)
    set (SOURCES ${SOURCES} win32.c)
else ()
    set (SOURCES ${SOURCES} unix.c)
endif ()

add_library (${TARGET_NAME} STATIC ${SOURCES})

if (WIN32)
    target_link_libraries (${TARGET_NAME} Ws2_32 winmm)
endif ()

if (MSVC)
    add_definitions("/D_WINSOCK_DEPRECATED_NO_WARNINGS")
endif ()

if (UNIX AND NOT APPLE)
    set_target_properties (${TARGET_NAME} PROPERTIES COMPILE_FLAGS "-fPIC")
endif ()

install (TARGETS ${TARGET_NAME}
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         RUNTIME DESTINATION bin
)
