# Installs indicators/ into a throwaway prefix and checks the files land under it.
#
# An install(DESTINATION) built out of ${CMAKE_INSTALL_PREFIX} is baked into the generated
# cmake_install.cmake as an absolute path, so it ignores whatever prefix the caller passes
# and always writes to the prefix the tree was configured with. Relative destinations are
# rebased against the caller's prefix, which is what every packaging and staging path needs.
#
# CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION is what makes that observable. Without it, a
# root-owned run of this script would happily write into the host's real /usr/share/latte
# and report success.
#
# Called as: cmake -DINSTALL_SCRIPT=<...> -DSTAGE=<...> -P indicators_install_prefix.cmake

if(NOT DEFINED INSTALL_SCRIPT OR NOT DEFINED STAGE)
    message(FATAL_ERROR "INSTALL_SCRIPT and STAGE must both be given")
endif()

# A leftover stage from an aborted run would satisfy the checks below without anything
# having been installed this time.
file(REMOVE_RECURSE "${STAGE}")

execute_process(
    COMMAND ${CMAKE_COMMAND} -DCMAKE_INSTALL_PREFIX=${STAGE}
                             -DCMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION=ON
                             -P ${INSTALL_SCRIPT}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr)

if(NOT _result EQUAL 0)
    message(FATAL_ERROR "installing into ${STAGE} failed (${_result}):\n${_stdout}${_stderr}")
endif()

# The relative destination is "share/latte/indicators", so at prefix /usr the installed tree
# is byte-identical to what the absolute form produced. Pinning the full path here is what
# catches a fix that relocates the packages while it makes them relocatable.
foreach(_indicator default org.kde.latte.plasma org.kde.latte.plasmatabstyle)
    set(_metadata "${STAGE}/share/latte/indicators/${_indicator}/metadata.json")
    if(NOT EXISTS "${_metadata}")
        message(FATAL_ERROR "${_metadata} is missing -- the indicator packages did not install to that path")
    endif()
endforeach()

# Only on success -- a failed run leaves the stage behind to look at.
file(REMOVE_RECURSE "${STAGE}")
