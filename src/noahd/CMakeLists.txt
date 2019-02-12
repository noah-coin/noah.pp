# generate pid.gen.hpp from pid.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/pid.gen.hpp
  MAIN_DEPENDENCY pid.idl
  COMMAND belt::idl ${CMAKE_CURRENT_SOURCE_DIR}/pid.idl ${CMAKE_CURRENT_SOURCE_DIR}/pid.gen.hpp
)

# define the executable
add_executable(noahd
    main.cpp)

# libraries this module links to
target_link_libraries(noahd PRIVATE
    belt::socket
    noah.pp
    mesh::mesh.pp
    mesh::systemutility
    belt::belt.pp
    publiq::blockchain
    belt::utility
    mesh::cryptoutility
    belt::log
    mesh::mlog
    publiq::blockchain
    Boost::filesystem
    Boost::program_options
    Boost::locale
    )

# what to do on make install
install(TARGETS noahd
        EXPORT noah.pp.package
        RUNTIME DESTINATION ${NOAHPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${NOAHPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${NOAHPP_INSTALL_DESTINATION_ARCHIVE})
