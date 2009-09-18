#
# Find the native apr includes and library
#

# This module defines
# APR_INCLUDE_DIR,
# APR_LIBRARIES, the libraries to link against to use apr.
# APR_FOUND, If false, do not try to use apr.

# also defined, but not for general use are
# APR_LIBRARY, where to find the apr library.
SET(APR_FOUND,"NO")

FIND_PATH(APR_INCLUDE_DIR apr.h
	/usr/include/
	/usr/local/include/
	/usr/local/apr/include/apr-1/
	)

FIND_LIBRARY(APR_LIBRARY apr-1
	/usr/lib64
	/usr/lib
	/usr/local/lib
	/usr/local/apr/lib
	)

IF (APR_LIBRARY)
	IF (APR_INCLUDE_DIR)
		SET( APR_FOUND "YES" )
		SET( APR_INCLUDE_DIR
			${APR_INCLUDE_DIR})
		SET( APR_LIBRARIES ${APR_LIBRARY} )
	ENDIF (APR_INCLUDE_DIR)
ENDIF (APR_LIBRARY)

MARK_AS_ADVANCED(
	APR_INCLUDE_DIR
	)

