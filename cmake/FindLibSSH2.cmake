# - Try to find the libssh2 library
# Once done this will define
#
# LIBSSH2_FOUND - system has the libssh2 library
# LIBSSH2_INCLUDE_DIR - the libssh2 include directory
# LIBSSH2_LIBRARY - the libssh2 library name

if(LIBSSH2_INCLUDE_DIR AND LIBSSH2_LIBRARY)
    set(LibSSH2_FIND_QUIETLY TRUE)
endif()

find_path(LIBSSH2_INCLUDE_DIR libssh2.h
        )

find_library(LIBSSH2_LIBRARY NAMES ssh2
        )

if(LIBSSH2_INCLUDE_DIR)
    file(STRINGS "${LIBSSH2_INCLUDE_DIR}/libssh2.h" libssh2_version_str REGEX "^#define[\t ]+LIBSSH2_VERSION_NUM[\t ]+0x[0-9][0-9][0-9][0-9][0-9][0-9].*")

    string(REGEX REPLACE "^.*LIBSSH2_VERSION_NUM[\t ]+0x([0-9][0-9]).*$" "\\1" LIBSSH2_VERSION_MAJOR "${libssh2_version_str}")
    string(REGEX REPLACE "^.*LIBSSH2_VERSION_NUM[\t ]+0x[0-9][0-9]([0-9][0-9]).*$" "\\1" LIBSSH2_VERSION_MINOR  "${libssh2_version_str}")
    string(REGEX REPLACE "^.*LIBSSH2_VERSION_NUM[\t ]+0x[0-9][0-9][0-9][0-9]([0-9][0-9]).*$" "\\1" LIBSSH2_VERSION_PATCH "${libssh2_version_str}")

    string(REGEX REPLACE "^0(.+)" "\\1" LIBSSH2_VERSION_MAJOR "${LIBSSH2_VERSION_MAJOR}")
    string(REGEX REPLACE "^0(.+)" "\\1" LIBSSH2_VERSION_MINOR "${LIBSSH2_VERSION_MINOR}")
    string(REGEX REPLACE "^0(.+)" "\\1" LIBSSH2_VERSION_PATCH "${LIBSSH2_VERSION_PATCH}")

    set(LIBSSH2_VERSION "${LIBSSH2_VERSION_MAJOR}.${LIBSSH2_VERSION_MINOR}.${LIBSSH2_VERSION_PATCH}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibSSH2 DEFAULT_MSG LIBSSH2_INCLUDE_DIR LIBSSH2_LIBRARY )

mark_as_advanced(LIBSSH2_INCLUDE_DIR LIBSSH2_LIBRARY LIBSSH2_VERSION_MAJOR LIBSSH2_VERSION_MINOR LIBSSH2_VERSION_PATCH LIBSSH2_VERSION)