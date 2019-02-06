if( WIN32 )
    SET(NOAHPP_INSTALL_DESTINATION_RUNTIME bin)
    SET(NOAHPP_INSTALL_DESTINATION_LIBRARY lib)
    SET(NOAHPP_INSTALL_DESTINATION_ARCHIVE lib)
    SET(NOAHPP_INSTALL_DESTINATION_INCLUDE include)
else( WIN32 )
    SET(NOAHPP_INSTALL_DESTINATION_RUNTIME bin/noah.pp)
    SET(NOAHPP_INSTALL_DESTINATION_LIBRARY lib/noah.pp)
    SET(NOAHPP_INSTALL_DESTINATION_ARCHIVE lib/noah.pp)
    SET(NOAHPP_INSTALL_DESTINATION_INCLUDE include/noah.pp)
endif( WIN32 )

if( NOT WIN32 )
    # don't skip the full RPATH for the build tree
    SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

    # when building, don't use the install RPATH already
    # (but later on when installing)
    SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

    SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/noah.pp;CMAKE_INSTALL_RPATH;${CMAKE_INSTALL_PREFIX}/lib/publiq.pp;${CMAKE_INSTALL_PREFIX}/lib/belt.pp;${CMAKE_INSTALL_PREFIX}/lib/mesh.pp")

    # not sure if there is any use from below command
    # add the automatically determined parts of the RPATH
    # which point to directories outside the build tree to the install RPATH
    #SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

include(cmake_utility/utility.cmake)

find_package(belt.pp)
find_package(mesh.pp)
find_package(publiq.pp)

add_subdirectory(noah.pp)
add_subdirectory(noahd)

# following is used for find_package functionality
install(FILES noah.pp-config.cmake DESTINATION ${NOAHPP_INSTALL_DESTINATION_LIBRARY})
install(EXPORT noah.pp.package NAMESPACE noah:: DESTINATION ${NOAHPP_INSTALL_DESTINATION_LIBRARY})
