# interface library for headers only module
add_library(noah.pp INTERFACE)

# modules linking to this library will include following
# directories. two options are provided for installed package
# and for in-tree source build
target_include_directories(noah.pp INTERFACE
    $<INSTALL_INTERFACE:include/noah.pp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>)

# libraries this module links to
target_link_libraries(noah.pp)

# what to do on make install
install(TARGETS noah.pp
        EXPORT noah.pp.package
        DESTINATION ${NOAHPP_INSTALL_DESTINATION_LIBRARY})

set(SRC_FILES
    global.hpp)

install(FILES
    ${SRC_FILES}
    DESTINATION ${NOAHPP_INSTALL_DESTINATION_INCLUDE}/noah.pp)

#
# add custom target simply for IDE to show the files
#
add_custom_target(noah.pp.include SOURCES
    ${SRC_FILES})
