# Run on every build (target pfm-version): writes version.h with "<major> · <short hash of HEAD>", e.g. 1 · e4547ff.
# The file only changes when HEAD does, so an unchanged HEAD rebuilds nothing.
execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY "${SRC}"
    OUTPUT_VARIABLE hash OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(hash)
    set(hash " · ${hash}")
endif() # not a git checkout: the major alone
file(CONFIGURE OUTPUT "${OUT}" CONTENT "#define PFM_VERSION \"${MAJOR}${hash}\"\n")
