file( GLOB CYTHON_FILES pyuda/cpyuda/*.pyx )

install( FILES ${CYTHON_FILES} pyuda/cpyuda/uda.pxd DESTINATION python_installer/pyuda/cpyuda )

file( GLOB INSTALL_FILES pyuda/*.py )

install( FILES ${INSTALL_FILES} DESTINATION python_installer/pyuda )

configure_file( ${CMAKE_CURRENT_LIST_DIR}/pyuda/_version.py.in ${CMAKE_CURRENT_BINARY_DIR}/pyuda/_version.py @ONLY )
install( FILES ${CMAKE_CURRENT_BINARY_DIR}/pyuda/_version.py DESTINATION python_installer/pyuda )

find_package( OpenSSL REQUIRED )
find_package( LibXml2 REQUIRED )
find_package( LibMemcached QUIET )
find_package( fmt REQUIRED )
find_package( TIRPC QUIET )

find_library( STDCXX_LIBRARY_FILE stdc++ HINTS ${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES} )
get_filename_component(STDCXX_LIB_DIR ${STDCXX_LIBRARY_FILE} DIRECTORY )

set( UDA_LIBS )
set( EXTRA_LIBS ${EXTRA_LIBS} fmt )

if( LIBMEMCACHED_FOUND )
  set( EXTRA_LIBS ${EXTRA_LIBS} memcached )
endif()
if( ENABLE_CAPNP )
  find_package( CapnProto REQUIRED )
  set( EXTRA_LIBS ${EXTRA_LIBS} capnp kj )
  set( UDA_LIBS ${UDA_LIBS} serialisation )
  set( CAPNP_FLAG "True" )
  get_filename_component( CAPNP_LIB_CMAKE_DIR "${CapnProto_DIR}" DIRECTORY )
  get_filename_component( CAPNP_LIB_DIR "${CAPNP_LIB_CMAKE_DIR}" DIRECTORY )
else()
  set( CAPNP_FLAG "False" )
endif()

get_target_property( FMT_INCLUDE_DIR fmt::fmt INTERFACE_INCLUDE_DIRECTORIES )
if( FMT_INCLUDE_DIR STREQUAL "FMT_INCLUDE_DIR-NOTFOUND" )
  get_filename_component( FMT_ROOT_DIR "${fmt_CONFIG}/../../../../" ABSOLUTE )
  set( FMT_INCLUDE_DIR ${FMT_ROOT_DIR}/include )
  set( FMT_LIB_DIR ${FMT_ROOT_DIR}/lib64 )
else()
  get_filename_component( FMT_LIB_DIR "${FMT_INCLUDE_DIR}/../lib" ABSOLUTE )
endif()


get_filename_component( OPENSSL_LIB_DIR "${OPENSSL_SSL_LIBRARY}" DIRECTORY )
get_filename_component( LIBXML_LIB_DIR "${LIBXML2_LIBRARIES}" DIRECTORY )

if( TIRPC_FOUND )
  get_filename_component( MAYBE_TIRPC "${TIRPC_INCLUDE_DIR}" NAME )
  if( MAYBE_TIRPC STREQUAL "tirpc" )
     set( TIRPC_PYUDA_INCLUDE_DIR ${TIRPC_INCLUDE_DIR} )
  else()
     set( TIRPC_PYUDA_INCLUDE_DIR ${TIRPC_INCLUDE_DIR}/tirpc )
  endif()
elseif( WIN32 AND NOT MINGW )
  # rpc/rpc.h is present in extlib/include next to xdr.h.
  # So, this not a clean solution: it just works.
  set( TIRPC_PYUDA_INCLUDE_DIR "${XDR_INCLUDE_DIR}" )
else()
  set( TIRPC_PYUDA_INCLUDE_DIR /usr/include/tirpc )
endif()

add_custom_command(
  OUTPUT
    "${CMAKE_CURRENT_BINARY_DIR}/setup.py"
  MAIN_DEPENDENCY
    "${CMAKE_CURRENT_LIST_DIR}/setup.py.in"
  DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/configure_setup.cmake"
    fmt::fmt
    LibXml2::LibXml2
  COMMAND
    ${CMAKE_COMMAND}
    -D "CMAKE_CURRENT_LIST_DIR=${CMAKE_CURRENT_LIST_DIR}"
    -D "CMAKE_CURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    -D "PROJECT_NAME=${PROJECT_NAME}"
    -D "PROJECT_VERSION=${PROJECT_VERSION}"
    -D "MSVC=${MSVC}"
    -D "FMT_INC=$<TARGET_PROPERTY:fmt::fmt,INTERFACE_INCLUDE_DIRECTORIES>"
    -D "FMT_LIB=$<TARGET_LINKER_FILE:fmt::fmt>"
    -D "XML2_INC=$<TARGET_PROPERTY:LibXml2::LibXml2,INTERFACE_INCLUDE_DIRECTORIES>"
    -D "XML2_LIB=$<TARGET_LINKER_FILE:LibXml2::LibXml2>"
    -P "${CMAKE_CURRENT_LIST_DIR}/configure_setup.cmake" )
add_custom_target(
  python_installer ALL
  DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/setup.py" )

install( FILES ${CMAKE_CURRENT_BINARY_DIR}/setup.py DESTINATION python_installer )
