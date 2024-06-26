########################################################################################################################
# Dependencies

if( WIN32 OR MINGW )
  find_package( XDR REQUIRED )
else()
  find_package( TIRPC QUIET )
  if( TIRPC_FOUND )
    add_definitions( -D__TIRPC__ )
  endif()
endif()

find_package( LibXml2 REQUIRED )

########################################################################################################################
# Sources

set( SRC_FILES
  parseIncludeFile.cpp
  struct.cpp
  accessors.cpp
  xdrUserDefinedData.cpp
)

include_directories(
  ${CMAKE_SOURCE_DIR}/source
  ${LIBXML2_INCLUDE_DIR}
)

if( MINGW OR WIN32 )
  include_directories( ${XDR_INCLUDE_DIR} )
#  add_compile_options( -Wno-error=use-after-free )
elseif( TIRPC_FOUND )
  include_directories( ${TIRPC_INCLUDE_DIR} )
endif()

########################################################################################################################
# Targets

if( NOT CLIENT_ONLY )
  add_library( structures-server-objects OBJECT ${SRC_FILES} )
  target_compile_definitions( structures-server-objects PRIVATE -DSERVERBUILD )
endif()

add_library( structures-client-objects OBJECT ${SRC_FILES} )

file( GLOB HEADER_FILES "*.h" )

install( FILES ${HEADER_FILES}
  DESTINATION include/uda/structures
)
