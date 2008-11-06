AC_INIT([openrobotix-playerstage], [0.1])
AM_INIT_AUTOMAKE([-Wall])

# Checks for programs.
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LIBTOOL

# Checks for libraries.
PKG_CHECK_MODULES(PLAYERCORE,
  [playercore],
  [have_playercore=yes],
  [])

PLAYER_PLUGIN_DIR=`$PKG_CONFIG --variable=prefix "playercore"`/lib/player
AC_SUBST(PLAYER_PLUGIN_DIR)

PKG_CHECK_MODULES(LIBV4L2,
  [libv4l2],
  [have_libv4l2=yes],
  [have_libv4l2=no])

PKG_CHECK_MODULES(LIBV4LCONVERT,
  [libv4lconvert],
  [have_libv4lconvert=yes],
  [have_libv4lconvert=no])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h string.h unistd.h])

# Checks for arguments
AC_ARG_ENABLE(drivers,
  AC_HELP_STRING([--disable-drivers], [Disable all drivers]),
  [enable_drivers=$enableval], [enable_drivers=yes])

AC_MSG_CHECKING(for enable drivers for the BeBot robot)
AC_ARG_ENABLE(bebot,
  AC_HELP_STRING([--disable-bebot], [Disable drivers for the BeBot robot]),
  [enable_bebot=$enableval],
  [enable_bebot=$enable_drivers])
AC_MSG_RESULT($enable_bebot)
AM_CONDITIONAL(WITH_BEBOT, test "x$enable_bebot" = "xyes")

AC_MSG_CHECKING(for enable driver for the Video4Linux2 interface)
if test "x$have_libv4l2" = "xyes" -a "x$have_libv4lconvert" = "xyes"; then
  AC_ARG_ENABLE(camerav4l2,
    AC_HELP_STRING([--disable-camerav4l2], [Disable driver for the Vide4Linux2 interface]),
    [enable_camerav4l2=$enableval],
    [enable_camerav4l2=$enable_drivers])
else
  enable_camerav4l2=no
fi
AC_MSG_RESULT($enable_camerav4l2)
AM_CONDITIONAL(WITH_CAMERAV4L2, test "x$enable_camerav4l2" = "xyes")

AC_CONFIG_FILES([Makefile config/Makefile drivers/Makefile drivers/bebot/Makefile drivers/camerav4l2/Makefile])
AC_OUTPUT
