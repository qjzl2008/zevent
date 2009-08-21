#
# Find the native apr includes and library
#

# This module defines
# APR-UTIL_INCLUDE_DIR,
# APR-UTIL_LIBRARIES, the libraries to link against to use apr.
# APR-UTIL_FOUND, If false, do not try to use apr.

# also defined, but not for general use are
# APR-UTIL_LIBRARY, where to find the apr library.

SET(APR-UTIL_FOUND,"NO")
FIND_PATH(APR-UTIL_INCLUDE_DIR apu.h
	/usr/local/apr/include/apr-1/
	/usr/local/apr-util/include/apr-1/
	)

FIND_LIBRARY(APR-UTIL_LIBRARY aprutil-1
	/usr/local/apr/lib
	/usr/local/apr-util/lib
	)

IF (APR-UTIL_LIBRARY)
	IF (APR-UTIL_INCLUDE_DIR)
		SET( APR-UTIL_FOUND "YES" )
		SET( APR-UTIL_INCLUDE_DIR
			${APR-UTIL_INCLUDE_DIR})
		SET( APR-UTIL_LIBRARIES ${APR-UTIL_LIBRARY} )
	ENDIF (APR-UTIL_INCLUDE_DIR)
ENDIF (APR-UTIL_LIBRARY)

MARK_AS_ADVANCED(
	APR-UTIL_INCLUDE_DIR
	)

