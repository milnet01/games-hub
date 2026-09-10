# GHUB-0191 -- locks gameshub_funding_entry()'s custom: parsing.
#
# Why this exists: the regex in cmake/funding.cmake pattern-matches one
# FUNDING.yml line rather than parsing YAML, and its optional brackets/quotes
# let a greedy capture swallow a comma-separated second URL or a trailing
# unquoted "]" instead of refusing the line. Contract: tests/funding-test.md.
#
# Includes cmake/funding.cmake directly -- the same file CMakeLists.txt
# includes -- rather than a copy of its rules, so this test can never drift
# from what the real build does.
#
# Run directly for diagnosable output:
#     cmake -P tests/funding-test.cmake
# Registered as the `funding` ctest case.

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/funding.cmake")

set(FAIL_COUNT 0)

function(check_entry description line expected)
    gameshub_funding_entry("${line}" actual)
    if(NOT actual STREQUAL expected)
        message("FAIL: ${description}")
        message("  input:    [${line}]")
        message("  expected: [${expected}]")
        message("  actual:   [${actual}]")
        math(EXPR bumped "${FAIL_COUNT} + 1")
        set(FAIL_COUNT "${bumped}" PARENT_SCOPE)
    else()
        message("pass: ${description}")
    endif()
endfunction()

# INV-1 / INV-2: a two-entry custom: list must never produce a donate entry
# -- it must return empty, which CMakeLists.txt's FATAL_ERROR turns into a
# stopped build -- whether the entries are quoted or not.
check_entry("INV-1 two-entry unquoted list stops the build"
            "custom: [https://a, https://b]" "")
check_entry("INV-2 two-entry quoted list stops the build"
            "custom: [\"https://a\", \"https://b\"]" "")

# INV-3: a single entry yields a clean URL -- no leftover bracket or quote --
# in every form GitHub's custom: key accepts.
check_entry("INV-3a bare URL, no brackets"
            "custom: https://a" "    { \"Tip page\", \"https://a\" },")
check_entry("INV-3b quoted URL, no brackets"
            "custom: \"https://a\"" "    { \"Tip page\", \"https://a\" },")
check_entry("INV-3c unquoted URL in a one-element list"
            "custom: [https://a]" "    { \"Tip page\", \"https://a\" },")
check_entry("INV-3d quoted URL in a one-element list"
            "custom: [\"https://a\"]" "    { \"Tip page\", \"https://a\" },")

if(FAIL_COUNT GREATER 0)
    message(FATAL_ERROR "funding-test: ${FAIL_COUNT} case(s) failed -- see above")
endif()
message("funding-test: all cases passed")
