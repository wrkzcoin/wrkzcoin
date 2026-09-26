# Stamps the commit being built into version.h as BUILD_COMMIT_ID. Runs on
# every build; configure_file only rewrites the header when the id changes.
#
# WRKZ_COMMIT_ID in the environment wins, for a build that cannot read its own
# checkout's git metadata - a container handed a linked worktree, whose .git
# points outside the mount, or a source archive with no .git at all.
#
# This used to ask `git describe --match "v*"`, which fails outright here: the
# release tags are named wrkzcoin_v*, so every build said nothing.
if (NOT "$ENV{WRKZ_COMMIT_ID}" STREQUAL "")
    set(VERSION "$ENV{WRKZ_COMMIT_ID}")
else ()
    execute_process(COMMAND "${GIT}" rev-parse --short=8 HEAD RESULT_VARIABLE RET OUTPUT_VARIABLE COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if (RET)
        message(WARNING "Cannot determine current revision. Make sure that you are building either from a Git working tree, or set WRKZ_COMMIT_ID.")
        set(VERSION "unknown")
    else ()
        # Tracked files only: a stray build directory or log is not a change
        # to what was built.
        execute_process(COMMAND "${GIT}" status --porcelain --untracked-files=no OUTPUT_VARIABLE CHANGES OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if (CHANGES STREQUAL "")
            set(VERSION "${COMMIT}")
        else ()
            set(VERSION "${COMMIT}-dirty")
        endif ()
    endif ()
endif ()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/config/version.h.in" "${TO}")
