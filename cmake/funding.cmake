# One line of .github/FUNDING.yml in, one donate entry out -- or an empty
# string for a line this build has no rule for, which CMakeLists.txt turns into
# a hard error. It lives in its own file so tests/funding-test.cmake can call
# the same rules the build does, rather than a copy of them (GHUB-0191).
#
# The handle-to-URL mapping is GitHub's, not this project's: FUNDING.yml
# stores account names for the platforms it knows and a full URL only for
# `custom`. That mapping has to live somewhere, and here is the one place
# it is not a copy of a link.
#
# A custom: URL may hold no comma and no closing bracket. That is what refuses
# a two-entry list, quoted or not, rather than merging it into one broken link,
# and what keeps a bracketed single URL from carrying its `]` (GHUB-0191).
function(gameshub_funding_entry line out_var)
    set(entry "")
    if(line MATCHES "^github:[ \t]*\\[?([A-Za-z0-9._-]+)\\]?$")
        set(entry "    { \"GitHub Sponsors\", \"https://github.com/sponsors/${CMAKE_MATCH_1}\" },")
    elseif(line MATCHES "^patreon:[ \t]*\\[?([A-Za-z0-9._-]+)\\]?$")
        set(entry "    { \"Patreon\", \"https://www.patreon.com/${CMAKE_MATCH_1}\" },")
    elseif(line MATCHES "^custom:[ \t]*\\[?\"?(https://[^]\",]+)\"?\\]?$")
        set(entry "    { \"Tip page\", \"${CMAKE_MATCH_1}\" },")
    endif()
    set(${out_var} "${entry}" PARENT_SCOPE)
endfunction()
