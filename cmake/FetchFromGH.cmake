# Fetch a single file from a GitHub repo.
#
# Usage:
#   github_fetch_file(
#     REPO        owner/repo            # or https://github.com/owner/repo(.git)
#     REF         v1.2.3                # branch, tag, or commit SHA
#     PATH        path/inside/repo/file.h
#     OUT         ${CMAKE_BINARY_DIR}/third_party/file.h
#     # Optional:
#     # TOKEN     ghp_xxx or GitHub App token (if needed for private repos or higher rate limits)
#   )
#
# Notes:
# - Prefers HTTPS raw download (fast, no 'git' required).
# - Falls back to sparse checkout (requires git ≥ 2.25).
# - For private repos via HTTPS, provide TOKEN (uses Authorization: Bearer ...).
function(github_fetch_file)
    set(options)
    set(oneValueArgs REPO REF PATH OUT TOKEN)
    cmake_parse_arguments(GFF "${options}" "${oneValueArgs}" "" ${ARGN})

    foreach(req REPO REF PATH OUT)
        if(NOT GFF_${req})
            message(FATAL_ERROR "[github_fetch_file] Missing required argument: ${req}")
        endif()
    endforeach()

    # Normalize output directory and short-circuit if already present
    get_filename_component(_outdir "${GFF_OUT}" DIRECTORY)
    file(MAKE_DIRECTORY "${_outdir}")
    if(EXISTS "${GFF_OUT}")
        message(STATUS "[github_fetch_file] Using existing file: ${GFF_OUT}")
        return()
    endif()

    # Derive owner/repo from various REPO formats
    set(_repo_in "${GFF_REPO}")
    set(_owner "")
    set(_name  "")

    if(_repo_in MATCHES "^https?://github\\.com/([^/]+)/([^/\\.]+)(\\.git)?/?$")
        set(_owner "${CMAKE_MATCH_1}")
        set(_name  "${CMAKE_MATCH_2}")
    elseif(_repo_in MATCHES "^([^/]+)/([^/]+)$")
        set(_owner "${CMAKE_MATCH_1}")
        set(_name  "${CMAKE_MATCH_2}")
    else()
        message(FATAL_ERROR "[github_fetch_file] REPO must be 'owner/repo' or 'https://github.com/owner/repo(.git)'. Got: ${_repo_in}")
    endif()

    # --- Strategy 1: raw.githubusercontent.com (fast path) ---
    set(_raw_url "https://raw.githubusercontent.com/${_owner}/${_name}/${GFF_REF}/${GFF_PATH}")
    set(_headers)
    if(GFF_TOKEN)
        list(APPEND _headers "Authorization: Bearer ${GFF_TOKEN}")
        # Optional but nice for audit/logs:
        list(APPEND _headers "Accept: application/vnd.github.v3.raw")
    endif()

    message(STATUS "[github_fetch_file] Trying raw download: ${_raw_url}")
    set(_dl_status)
    if(GFF_TOKEN)
        file(DOWNLOAD "${_raw_url}" "${GFF_OUT}"
            HTTPHEADER "${_headers}"
            STATUS _dl_status
            SHOW_PROGRESS)
    else()
        file(DOWNLOAD "${_raw_url}" "${GFF_OUT}"
            STATUS _dl_status
            SHOW_PROGRESS)
    endif()

    list(GET _dl_status 0 _dl_code)
    if(_dl_code EQUAL 0 AND EXISTS "${GFF_OUT}")
        message(STATUS "[github_fetch_file] Fetched via raw.githubusercontent.com → ${GFF_OUT}")
        return()
    else()
        list(LENGTH _dl_status _dl_len)
        if(_dl_len GREATER 1)
            list(GET _dl_status 1 _dl_msg)
        else()
            set(_dl_msg "unknown error")
        endif()
        message(STATUS "[github_fetch_file] Raw download unavailable; falling back. (${_dl_msg})")
    endif()

    # --- Strategy 2: shallow sparse checkout (robust fallback) ---
    find_program(GIT_EXECUTABLE git)
    if(NOT GIT_EXECUTABLE)
        message(FATAL_ERROR "[github_fetch_file] 'git' not found and raw download failed.")
    endif()

    set(_tmpdir "${CMAKE_BINARY_DIR}/_gh_fetch_${_owner}_${_name}")
    set(_wc     "${_tmpdir}/wc")
    file(REMOVE_RECURSE "${_wc}")
    file(MAKE_DIRECTORY "${_wc}")

    # Build remote URL (inject token if provided to access private repos over HTTPS)
    if(GFF_TOKEN)
        # Token in header is not supported by git; embed in URL (safe enough for CI logs if you avoid echoing it).
        # Use x-access-token as user to avoid leaking username.
        set(_remote "https://x-access-token:${GFF_TOKEN}@github.com/${_owner}/${_name}.git")
    else()
        set(_remote "https://github.com/${_owner}/${_name}.git")
    endif()

    message(STATUS "[github_fetch_file] Falling back to shallow sparse checkout …")
    execute_process(COMMAND "${GIT_EXECUTABLE}" init
        RESULT_VARIABLE _res_init
        WORKING_DIRECTORY "${_wc}")
    if(NOT _res_init EQUAL 0)
        message(FATAL_ERROR "[github_fetch_file] git init failed.")
    endif()

    execute_process(COMMAND "${GIT_EXECUTABLE}" remote add origin "${_remote}"
        RESULT_VARIABLE _res_remote
        WORKING_DIRECTORY "${_wc}")
    if(NOT _res_remote EQUAL 0)
        message(FATAL_ERROR "[github_fetch_file] git remote add failed.")
    endif()

    execute_process(COMMAND "${GIT_EXECUTABLE}" sparse-checkout init --cone
        RESULT_VARIABLE _res_spinit
        WORKING_DIRECTORY "${_wc}")
    if(NOT _res_spinit EQUAL 0)
        message(FATAL_ERROR "[github_fetch_file] git sparse-checkout init failed (need git ≥ 2.25).")
    endif()

    execute_process(COMMAND "${GIT_EXECUTABLE}" sparse-checkout set "${GFF_PATH}"
        RESULT_VARIABLE _res_spset
        WORKING_DIRECTORY "${_wc}")
    if(NOT _res_spset EQUAL 0)
        message(FATAL_ERROR "[github_fetch_file] git sparse-checkout set failed for path: ${GFF_PATH}")
    endif()

    execute_process(COMMAND "${GIT_EXECUTABLE}" fetch --depth=1 --filter=blob:none origin "${GFF_REF}"
        RESULT_VARIABLE _res_fetch
        WORKING_DIRECTORY "${_wc}")
    if(NOT _res_fetch EQUAL 0)
        message(FATAL_ERROR "[github_fetch_file] git fetch failed for ref ${GFF_REF}.")
    endif()

    execute_process(COMMAND "${GIT_EXECUTABLE}" checkout --force FETCH_HEAD
        RESULT_VARIABLE _res_co
        WORKING_DIRECTORY "${_wc}")
    if(NOT _res_co EQUAL 0)
        message(FATAL_ERROR "[github_fetch_file] git checkout failed.")
    endif()

    set(_src "${_wc}/${GFF_PATH}")
    if(EXISTS "${_src}")
        file(COPY_FILE "${_src}" "${GFF_OUT}")
        message(STATUS "[github_fetch_file] Fetched via sparse checkout → ${GFF_OUT}")
    else()
        message(FATAL_ERROR "[github_fetch_file] Path not found at ref: ${GFF_PATH} @ ${GFF_REF}")
    endif()
endfunction()
