AC_PREREQ([2.69])
AC_INIT([xdag], [0.2.1], [cz.worker@gmail.com])

AC_CANONICAL_HOST
HOST_SUFFIX=""
HOST_OS=""

# Detect the target system
case "${host_os}" in
    linux*)
        HOST_SUFFIX=""
        HOST_OS="linux"
        ;;
    cygwin*|mingw*)
        HOST_SUFFIX="-win"
        HOST_OS="win"
        ;;
    darwin*)
        HOST_SUFFIX="-mac"
        HOST_OS="macos"
        ;;
    *)
        AC_MSG_ERROR(["OS $host_os is not supported"])
        ;;
esac

AC_SUBST([HOST_OS])
AC_SUBST([HOST_SUFFIX])

AC_CONFIG_SRCDIR([../client/version.h])

# config.h
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign subdir-objects no-dependencies -Wall])

# Checks for programs.
AC_PROG_CC
AM_PROG_AS

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h memory.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h readline/readline.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([ftruncate gethostbyname gettimeofday localtime_r memmove memset munmap socket strchr strdup strerror readline])

# for macos
LIBCRYPTO_CFLAGS="-I/usr/local/opt/openssl/include"
AC_SUBST(LIBCRYPTO_CFLAGS)

# Checks for libraries.
AC_CHECK_LIB([crypto], [main])
AC_CHECK_LIB([m], [main])
AC_CHECK_LIB([pthread], [main])
AC_CHECK_LIB([ssl], [main])
AC_CHECK_LIB([readline], [main])

AC_CONFIG_FILES([
    Makefile
])

AC_OUTPUT

