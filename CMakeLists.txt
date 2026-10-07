# ---------------------------------------------------------------------------
# SAC - Seismic Analysis Code (Community Edition)
#
# Modern CMake build.  This replaces the Visual Studio 2013 project and the
# hand-written batch scripts that used to live in win/.
#
# Typical use, from an MSYS2 CLANGARM64 (or MINGW64/UCRT64) shell:
#
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# The auxiliary data directory is staged next to sac.exe so that a fresh
# build tree is runnable without installing.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.20)

project(sac
    VERSION 103.0
    DESCRIPTION "SAC - Seismic Analysis Code (Community Edition)"
    LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR
        "This CMake build currently only supports Windows.  Use the autotools "
        "build (configure && make) on other platforms.")
endif()

enable_language(RC)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_CXX_STANDARD 11)

# The sources are old C and produce a lot of noise; keep warnings visible but
# do not let them stop the build.
if(MSVC)
    add_compile_options(/W0 /wd4996)
else()
    add_compile_options(-w)
endif()

# MinGW-w64 declares asprintf, vasprintf, strsep and friends only when
# _GNU_SOURCE is defined, even though libmingwex provides the symbols.  The
# autotools build defines this for Linux; Windows needs it for the same reason.
if(MINGW)
    add_compile_definitions(_GNU_SOURCE)
endif()

# Functions MinGW does not provide at all.  SAC carries its own portable
# implementations for these (see win/config.h.in).
foreach(fn MKSTEMPS INDEX RINDEX STRLCPY STRLCAT BASENAME DIRNAME
           BZERO FMEMOPEN WORDEXP)
    set(MISSING_FUNC_${fn} 1)
endforeach()

# Capabilities the C library and its headers do provide.  These mirror what
# autoconf (and inc/config_win.h before it) detected for this toolchain.
foreach(fn FMAX FMIN LROUND PUTENV SLEEP STRTOF TOLOWER TOUPPER VASPRINTF)
    set(HAVE_FUNC_${fn} 1)
endforeach()
foreach(h INTTYPES_H LIMITS_H STDINT_H STDIO_H STDLIB_H STRINGS_H STRING_H
          SYS_STAT_H SYS_TYPES_H UNISTD_H)
    set(HAVE_${h} 1)
endforeach()
set(STDC_HEADERS 1)

# There is no supported database layer on Windows (see inc/config.h.in).
set(DATABASE_DISABLED 1)
set(HAVE_LIBRPC_DISABLED 1)

# Force-include the shim that keeps the vendored libraries (evalresp/mxml,
# libmseed, sacio/time64, linenoise) free of Windows #ifdefs of their own.
if(NOT MSVC)
    add_compile_options(-include "${PROJECT_SOURCE_DIR}/win/compat.h")
endif()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
set(SAC_SACAUX "${CMAKE_INSTALL_PREFIX}/sacaux" CACHE PATH
    "Directory SAC looks in for auxiliary data (SACAUX)")
option(SAC_STAGE_AUX "Copy sacaux/ next to sac.exe after building" ON)
option(SAC_BUILD_TESTS "Also build the sacio/fern/libmseed test programs" OFF)
option(SAC_STATIC_DEPS
    "Link libxml2, zlib and curl statically so sac.exe runs without MSYS2" ON)

if(SAC_STATIC_DEPS)
    # Without these, the curl and libxml2 headers declare every function
    # __declspec(dllimport), which makes the compiler emit references to the
    # DLL import symbols that the static archives do not define.
    add_compile_definitions(CURL_STATICLIB LIBXML_STATIC)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)

pkg_check_modules(XML  REQUIRED IMPORTED_TARGET libxml-2.0)
pkg_check_modules(ZLIB REQUIRED IMPORTED_TARGET zlib)
set(HAVE_LIBM 1)

find_package(CURL)
if(CURL_FOUND)
    set(HAVE_LIBCURL 1)
    # Capabilities reported by libcurl's own configuration.
    set(LIBCURL_FEATURE_ASYNCHDNS 1)
    set(LIBCURL_FEATURE_IPV6 1)
    set(LIBCURL_FEATURE_LIBZ 1)
    set(LIBCURL_FEATURE_SSL 1)
    foreach(p DICT FILE FTP FTPS HTTP HTTPS IMAP LDAP POP3 RTSP SMTP TELNET TFTP)
        set(LIBCURL_PROTOCOL_${p} 1)
    endforeach()
endif()

# ---------------------------------------------------------------------------
# config.h
#
# One generated header replaces the two hand-maintained ones
# (inc/config_win.h for MSVC, an autoconf header for MinGW/Cygwin).
# ---------------------------------------------------------------------------
string(TIMESTAMP SAC_BUILD_DATE "%Y-%m-%d" UTC)

# SAC is a command line program that opens its own plot windows, so the
# executable is a console application and src/win/win.cpp uses its main() entry
# point.  Defining GUI_APP instead selects win.cpp's WinMain() entry point,
# which allocates a private console and detaches SAC from the terminal that
# started it -- that was the old Visual Studio behaviour, kept only as an
# option.
option(SAC_GUI_SUBSYSTEM
    "Link with the Windows GUI subsystem and allocate a private console" OFF)
if(SAC_GUI_SUBSYSTEM)
    set(GUI_APP 1)
endif()
set(SAC_CONFIG_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${SAC_CONFIG_DIR}")

configure_file(
    "${PROJECT_SOURCE_DIR}/win/config.h.in"
    "${SAC_CONFIG_DIR}/config.h"
    @ONLY)

# ---------------------------------------------------------------------------
# Build-time patches for vendored libraries
#
# The vendored sources (libmseed, evalresp/mxml, fern) stay byte-for-byte
# identical to upstream.  Where Windows needs a change, the component is copied
# into the build tree and a patch is applied there.  "git apply" refuses to
# guess, so if an upstream update moves the code being patched the build stops
# with an explicit message instead of silently dropping the fix.
# ---------------------------------------------------------------------------
set(SAC_PATCH_ROOT "${CMAKE_BINARY_DIR}/vendored")

# git is used purely as a patch tool here; it does not have to be on PATH
# (MSYS2 shells, for instance, often do not have it).
find_package(Git REQUIRED)

# git apply resolves "--directory" relative to the repository root, and refuses
# to touch the build tree when run from inside it, so patch from the top level.
file(RELATIVE_PATH SAC_PATCH_REL "${PROJECT_SOURCE_DIR}" "${SAC_PATCH_ROOT}")

function(sac_patch_component component)
    get_filename_component(_dest "${SAC_PATCH_ROOT}/${component}" DIRECTORY)
    file(COPY "${PROJECT_SOURCE_DIR}/${component}" DESTINATION "${_dest}")

    foreach(_patch ${ARGN})
        set(_patch_file "${PROJECT_SOURCE_DIR}/win/patches/${_patch}")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" apply -p1
                    --directory=${SAC_PATCH_REL}
                    --whitespace=nowarn "${_patch_file}"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            RESULT_VARIABLE _rc
            ERROR_VARIABLE _err)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR
                "Could not apply win/patches/${_patch} to the vendored copy of "
                "${component}.\nThis means the vendored code has changed and the "
                "patch needs to be refreshed.\n${_err}")
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_patch_file}")
    endforeach()
endfunction()

sac_patch_component(libmseed                libmseed-stat.patch)
sac_patch_component(fern                    fern-arg-double.patch)
sac_patch_component(evalresp/libsrc/mxml   mxml-ssize_t.patch)
sac_patch_component(sacio                   time64-prid64.patch
                                            sacio-binary-mode.patch)

# ---------------------------------------------------------------------------
# In-tree support libraries
# ---------------------------------------------------------------------------
# ${SAC_PATCH_ROOT} comes first so that root-relative includes of a patched
# component ("libmseed/libmseed.h", ...) pick up the patched copy.
set(SAC_INCLUDE_DIRS
    "${SAC_CONFIG_DIR}"                 # generated config.h
    "${PROJECT_SOURCE_DIR}/win/compat"  # shim headers that Windows lacks
    "${SAC_PATCH_ROOT}"
    "${SAC_PATCH_ROOT}/libmseed"
    "${SAC_PATCH_ROOT}/fern"
    "${SAC_PATCH_ROOT}/sacio"           # patched time64.h wins over the source tree
    "${PROJECT_SOURCE_DIR}/inc"
    "${PROJECT_SOURCE_DIR}/sacio"          # vendored time64.h, included by the shim
    "${PROJECT_SOURCE_DIR}/src"            # sources include "eval/...", "string/..."
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/evalresp/libsrc"
    "${PROJECT_SOURCE_DIR}/evalresp/libsrc/mxml"   # <mxml.h>, used by stationxml2resp
    ${XML_INCLUDE_DIRS}                 # libxml2 headers live in include/libxml2
    ${ZLIB_INCLUDE_DIRS})

if(CURL_FOUND)
    list(APPEND SAC_INCLUDE_DIRS ${CURL_INCLUDE_DIRS})
endif()

# libmseed (patched copy: see the vendored patches above)
file(GLOB LIBMSDEED_SOURCES CONFIGURE_DEPENDS "${SAC_PATCH_ROOT}/libmseed/*.c")
add_library(mseed STATIC ${LIBMSDEED_SOURCES})
target_include_directories(mseed PUBLIC "${PROJECT_SOURCE_DIR}/libmseed")

# sacio.  Compiled from the patched copy (see the vendored patches above):
# sacio-binary-mode.patch changes sacio.c itself, so the sources -- not just the
# headers -- have to come from the patched tree.
file(GLOB SACIO_SOURCES CONFIGURE_DEPENDS "${SAC_PATCH_ROOT}/sacio/*.c")
add_library(sacio_bsd STATIC ${SACIO_SOURCES})
target_include_directories(sacio_bsd PUBLIC "${SAC_PATCH_ROOT}/sacio")

# fern.  These are the explicit lists from fern/Makefile.am, not a glob: the
# directory also contains fern.c, the standalone command line program (it
# defines main() and its own error()), which must never be linked into SAC --
# it collides with SAC's error() from src/msg/inimsg.c.
set(SAC_FERN_LIB
    cJSON.c datareq.c event.c json.c meta.c miniseed_sac.c quake_xml.c
    request.c response.c slurp.c station.c stationreq.c strip.c xml.c)
set(SAC_PILE_LIB chash.c array.c cprint.c)

# Compile them from the patched copy (see the vendored patches above).
list(TRANSFORM SAC_FERN_LIB PREPEND "${SAC_PATCH_ROOT}/fern/")
list(TRANSFORM SAC_PILE_LIB PREPEND "${SAC_PATCH_ROOT}/fern/")

add_library(fern STATIC ${SAC_FERN_LIB})
target_include_directories(fern PUBLIC "${SAC_PATCH_ROOT}/fern")
add_library(pile STATIC ${SAC_PILE_LIB})
target_include_directories(pile PUBLIC "${SAC_PATCH_ROOT}/fern")

# evalresp and its bundled helpers.  The examples/ and test/ directories are
# deliberately not globbed: they contain standalone programs (and, for
# evalresp_log, a syslog example that does not build on Windows).
file(GLOB EVALRESP_SOURCES      CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/evalresp/libsrc/evalresp/*.c")
# The StationXML support lives one directory down and is referenced by
# evalresp/input.c (evalresp_xml_stream_to_resp_file), so it must be compiled
# too.  examples/ is deliberately not globbed: it holds standalone programs.
file(GLOB EVALRESP_XML_SOURCES  CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/evalresp/libsrc/evalresp/stationxml2resp/*.c")
file(GLOB EVALRESP_LOG_SOURCES  CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/evalresp/libsrc/evalresp_log/*.c")
file(GLOB MXML_SOURCES          CONFIGURE_DEPENDS "${SAC_PATCH_ROOT}/evalresp/libsrc/mxml/*.c")
# mmd.c, mxmldoc.c and testmxml.c are standalone programs that happen to live in
# the same directory; linking them in would pull a second main() into SAC and,
# with a console entry point, the wrong one would run.
list(FILTER MXML_SOURCES EXCLUDE REGEX "/(mmd|mxmldoc|testmxml)\\.c$")
file(GLOB SPLINE_SOURCES        CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/evalresp/libsrc/spline/*.c")

add_library(evalresp STATIC ${EVALRESP_SOURCES} ${EVALRESP_XML_SOURCES})
add_library(evalresp_log STATIC ${EVALRESP_LOG_SOURCES})
add_library(mxmlev STATIC ${MXML_SOURCES})
add_library(spline STATIC ${SPLINE_SOURCES})

# Every component includes its headers through repository-root-relative paths
# (e.g. "sacio/sacio.h", "libmseed/libmseed.h"), so they all need the root and
# the generated config.h on their include path.
foreach(t mseed sacio_bsd fern pile evalresp evalresp_log mxmlev spline)
    target_include_directories(${t} PUBLIC ${SAC_INCLUDE_DIRS})
endforeach()

# Shims for things Windows does not have at all (see win/compat/).
add_library(win_compat STATIC
    "${PROJECT_SOURCE_DIR}/win/compat/bison_trace.c"
    "${PROJECT_SOURCE_DIR}/win/compat/glob.c"
    "${PROJECT_SOURCE_DIR}/win/compat/ioctl.c"
    "${PROJECT_SOURCE_DIR}/win/compat/strsep.c"
    "${PROJECT_SOURCE_DIR}/win/compat/time_r.c")
target_include_directories(win_compat PUBLIC "${PROJECT_SOURCE_DIR}/win/compat")
target_include_directories(win_compat PRIVATE ${SAC_INCLUDE_DIRS})

# ---------------------------------------------------------------------------
# SAC itself
#
# The source lists come from win/sources.cmake, which is generated from
# src/Makefile.am (see win/gen-sources.ps1) so that the two build systems
# cannot drift while both are present.
# ---------------------------------------------------------------------------
include("${PROJECT_SOURCE_DIR}/win/sources.cmake")

# linenoise is vendored for POSIX and needs <termios.h>.  The Windows build has
# no line editing (configure reports "Editing: none" for mingw) and the Visual
# Studio project did not compile it either, so drop it from the list.
list(REMOVE_ITEM SAC_LIBSAC_ALL src/co/linenoise.c)

add_library(sac_libs STATIC
    ${SAC_LIBSAC_ALL}
    ${SAC_LIBSGF}
    ${SAC_LIBICM}
    ${SAC_LIBMSG}
    ${SAC_LIBVARS}
    ${SAC_LIBTURTLE}
    ${SAC_LIBSACIO}
    ${SAC_LIBSMNONORACLE}
    ${SAC_LIBSACORACLESTUB})

target_include_directories(sac_libs PUBLIC ${SAC_INCLUDE_DIRS})

# src/bs/iniam.c talks to libxml2 directly, so the include directories above
# must cover it.  Linking is left to the sac target so that the static and
# shared configurations can be expressed in one place.

if(SAC_GUI_SUBSYSTEM)
    set(SAC_EXE_SUBSYSTEM WIN32)
endif()

add_executable(sac ${SAC_EXE_SUBSYSTEM} ${SAC_EXE_SOURCES} ${SAC_LIBSAC_WIN})
target_include_directories(sac PRIVATE ${SAC_INCLUDE_DIRS})

target_link_libraries(sac PRIVATE
    sac_libs
    win_compat
    fern pile
    mseed
    sacio_bsd
    evalresp evalresp_log mxmlev spline)

# Third-party libraries.  With SAC_STATIC_DEPS the complete static link chain
# comes from pkg-config (libcurl alone pulls in TLS, HTTP/2, brotli, zstd, ...),
# because a plain -lcurl / -lxml2 would otherwise resolve to the MSYS2 DLL
# import libraries and the result could not be redistributed.
if(SAC_STATIC_DEPS)
    execute_process(
        COMMAND "${PKG_CONFIG_EXECUTABLE}" --static --libs libcurl libxml-2.0 zlib
        OUTPUT_VARIABLE SAC_DEPS_STATIC
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR
            "pkg-config --static failed for libcurl/libxml-2.0/zlib, so a "
            "self-contained sac.exe cannot be produced.  Configure with "
            "-DSAC_STATIC_DEPS=OFF to link the shared MSYS2 libraries instead.")
    endif()
    separate_arguments(SAC_DEPS_STATIC UNIX_COMMAND "${SAC_DEPS_STATIC}")
    target_link_libraries(sac PRIVATE -Wl,-Bstatic ${SAC_DEPS_STATIC} -Wl,-Bdynamic)
else()
    target_link_libraries(sac PRIVATE PkgConfig::XML PkgConfig::ZLIB)
    if(CURL_FOUND)
        target_link_libraries(sac PRIVATE CURL::libcurl)
    endif()
endif()

target_link_libraries(sac PRIVATE gdiplus ws2_32)

# Static runtime so the resulting sac.exe does not need MinGW DLLs.
if(MINGW)
    target_link_options(sac PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# ---------------------------------------------------------------------------
# Utility programs (utils/)
#
# The autotools build has utils in SUBDIRS and installs these eight
# bin_PROGRAMS.  The Windows build produces them too: SAC itself shells out to
# "sgftops" to turn a .sgf file into PostScript (src/gam/xprint.c), so the
# program has to be on PATH for SAC's print/PostScript output to work.
# ---------------------------------------------------------------------------
option(SAC_BUILD_UTILS "Also build the utils/ command line programs" ON)

set(SAC_UTILS_PROGRAMS sgftops sgfswap saclst sacswap sacset sacdiff bbfswap unvis)
# These read and write SAC files through sacio; the other three are standalone
# (sgftops and sgfswap touch .sgf files, unvis is a text tool).
set(SAC_UTILS_WITH_SACIO saclst sacswap sacset sacdiff bbfswap)

if(SAC_BUILD_UTILS)
    # The autotools build links the utils against ../src/libsacio.a, which is
    # sacio_bsd plus the three sources below (src/Makefile.am:
    # libsacio_a_SOURCES = ucf/distaz.c co/math.c vars/bbf.c).  distaz() is
    # required because sacio.c's update_distaz() calls it, and bbf.c backs
    # bbfswap.  win_compat supplies localtime_r(), which sacio's time64.c uses.
    add_library(sacio_sac STATIC
        "${PROJECT_SOURCE_DIR}/src/ucf/distaz.c"
        "${PROJECT_SOURCE_DIR}/src/co/math.c"
        "${PROJECT_SOURCE_DIR}/src/vars/bbf.c")
    target_include_directories(sacio_sac PUBLIC ${SAC_INCLUDE_DIRS})
    target_link_libraries(sacio_sac PUBLIC sacio_bsd win_compat)

    foreach(_prog ${SAC_UTILS_PROGRAMS})
        add_executable(${_prog} "${PROJECT_SOURCE_DIR}/utils/${_prog}.c")
        target_include_directories(${_prog} PRIVATE ${SAC_INCLUDE_DIRS})
        if(_prog IN_LIST SAC_UTILS_WITH_SACIO)
            target_link_libraries(${_prog} PRIVATE sacio_sac)
        endif()
        if(MINGW)
            target_link_options(${_prog} PRIVATE -static -static-libgcc)
        endif()
    endforeach()
endif()

# ---------------------------------------------------------------------------
# Stage the auxiliary data next to the executable so the build tree is usable
# as-is (this is what win/build_win.bat used to do by hand).
# ---------------------------------------------------------------------------
if(SAC_STAGE_AUX)
    add_custom_command(TARGET sac POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${PROJECT_SOURCE_DIR}/sacaux"
                "$<TARGET_FILE_DIR:sac>/sacaux"
        COMMENT "Staging sacaux next to sac.exe")
endif()

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------
install(TARGETS sac RUNTIME DESTINATION bin)
if(SAC_BUILD_UTILS)
    install(TARGETS ${SAC_UTILS_PROGRAMS} RUNTIME DESTINATION bin)
endif()
install(DIRECTORY "${PROJECT_SOURCE_DIR}/sacaux" DESTINATION .)

# ---------------------------------------------------------------------------
# Distribution
#
# Assembles a self-contained tree (sac.exe plus the sacaux data) and zips it,
# which is the form the Windows build is shipped in.
# ---------------------------------------------------------------------------
set(SAC_DIST_DIR "${CMAKE_BINARY_DIR}/dist")
set(SAC_DIST_NAME "sac-${PROJECT_VERSION}-windows-${CMAKE_SYSTEM_PROCESSOR}")

# The utils programs ship next to sac.exe.  sgftops is looked up through PATH
# (SAC runs "sgftops ..." with system()), so it has to be reachable from there.
set(SAC_DIST_UTIL_STEPS "")
set(SAC_DIST_DEPENDS sac)
if(SAC_BUILD_UTILS)
    foreach(_prog ${SAC_UTILS_PROGRAMS})
        list(APPEND SAC_DIST_UTIL_STEPS
            COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${_prog}>" "${SAC_DIST_DIR}/")
        list(APPEND SAC_DIST_DEPENDS ${_prog})
    endforeach()
endif()

# Everything sac.exe needs is either a Windows system DLL or linked in
# statically, except libc++: MSYS2 builds it as a DLL and the compiler adds its
# own implicit -lc++ that cannot be redirected to libc++.a.  Ship it.
get_filename_component(SAC_TOOLCHAIN_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(SAC_RUNTIME_DLLS "")
foreach(_dll libc++.dll libc++abi.dll libunwind.dll)
    if(EXISTS "${SAC_TOOLCHAIN_BIN}/${_dll}")
        list(APPEND SAC_RUNTIME_DLLS "${SAC_TOOLCHAIN_BIN}/${_dll}")
    endif()
endforeach()

add_custom_target(dist
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${SAC_DIST_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SAC_DIST_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:sac>" "${SAC_DIST_DIR}/"
    ${SAC_DIST_UTIL_STEPS}
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${SAC_RUNTIME_DLLS} "${SAC_DIST_DIR}/"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${PROJECT_SOURCE_DIR}/sacaux" "${SAC_DIST_DIR}/sacaux"
    COMMAND "${CMAKE_COMMAND}" -E copy "${PROJECT_SOURCE_DIR}/License" "${SAC_DIST_DIR}/"
    COMMAND "${CMAKE_COMMAND}" -E copy "${PROJECT_SOURCE_DIR}/README.md" "${SAC_DIST_DIR}/"
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${CMAKE_BINARY_DIR}/${SAC_DIST_NAME}.zip"
            --format=zip -- "${SAC_DIST_DIR}"
    DEPENDS ${SAC_DIST_DEPENDS}
    COMMENT "Assembling ${SAC_DIST_NAME}.zip"
    VERBATIM)
