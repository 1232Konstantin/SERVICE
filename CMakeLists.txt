cmake_minimum_required(VERSION 3.10)

set(PATCH_VERSION "1" CACHE INTERNAL "Patch version")  
set(PROJECT_VESRION 0.0.${PATCH_VERSION})    

#for VS CODE LOCAL  in my PC
#set (BOOST_ROOT "G:/boost_1_83_0/boost_1_83_0/") # Путь к библиотеке Boost
#set (Boost_NO_SYSTEM_PATHS ON)
#set (Boost_USE_STATIC_LIBS ON)
#set (Boost_USE_STATIC_RUNTIME OFF)
#set (BOOST_ALL_DYN_LINK OFF)



project(msg_exchange VERSION ${PROJECT_VESRION})

#find_package(Boost 1.83 REQUIRED)
find_package(Boost 1.74 REQUIRED)


add_library(commonlib common.cpp) 
add_library(serverlib server.cpp) 
add_executable(msg_exchange main.cpp )

set_target_properties(msg_exchange  PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

#for GITHUB
set_target_properties(commonlib  PROPERTIES
    COMPILE_DEFINITIONS BOOST_TEST_DYN_LINK
    INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR}
)


set_target_properties(serverlib  PROPERTIES
    COMPILE_DEFINITIONS BOOST_TEST_DYN_LINK
    INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR}
)

#for VS CODE LOCAL  in my PC
#set_target_properties(msg_exchange  PROPERTIES
#    INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR} 
#)

target_link_libraries(commonlib
    ${Boost_LIBRARIES}
)

target_link_libraries(serverlib
    ${Boost_LIBRARIES}
    commonlib
)

target_link_libraries(msg_exchange
    commonlib
    serverlib
)


if(WIN32)
  target_link_libraries(msg_exchange wsock32 ws2_32)
endif()


install(TARGETS msg_exchange RUNTIME DESTINATION bin)

set(CPACK_GENERATOR DEB)

set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")

set(CPACK_PACKAGE_CONTACT inotunderstand@yandex.com)

include(CPack)

