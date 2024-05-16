set( EXTRA_LIBS )

macro(_format_args _name _inc_dir_name _lib_dir_name)
    set( _inc "${${_name}_INC}" )
    set( _lib "${${_name}_LIB}" )
    message( STATUS "${_name}_INC: ${_inc}" )
    message( STATUS "${_name}_LIB: ${_lib}" )
    set( ${_inc_dir_name} "${_inc}" )
    get_filename_component( ${_lib_dir_name} "${_lib}" DIRECTORY )
    get_filename_component( _lib "${_lib}" NAME_WE )
    if ( NOT MSVC AND "${_lib}" MATCHES "^lib(.+)$")
        set( _lib "${CMAKE_MATCH_1}" )
    endif()
    list( APPEND EXTRA_LIBS "${_lib}" )
    unset( _inc )
    unset( _lib )
endmacro(_format_args)

_format_args(FMT FMT_INCLUDE_DIR FMT_LIB_DIR)
_format_args(XML2 LIBXML2_INCLUDE_DIR LIBXML_LIB_DIR)

configure_file( ${CMAKE_CURRENT_LIST_DIR}/setup.py.in ${CMAKE_CURRENT_BINARY_DIR}/setup.py @ONLY )

