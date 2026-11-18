# Verify exact unmodified MIT-licensed upstream inputs on host and SDK builds.
function(tv4_check_cjson_inputs)
foreach(entry IN ITEMS
  "cJSON.c|298581a04a36c0165da4b0aade235c23088cb2faa58651d720ea2f3706ed0b0d"
  "cJSON.h|25b0145150d500498e4d209cec69c18c42cf818bffcc54690be3b895a2a16dee"
  "LICENSE|a36dda207c36db5818729c54e7ad4e8b0c6fba847491ba64f372c1a2037b6d5c")
  string(REPLACE "|" ";" fields "${entry}")
  list(GET fields 0 name)
  list(GET fields 1 expected)
  file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/${name}" actual)
  if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "Unreviewed cJSON upstream change: ${name}")
  endif()
endforeach()
endfunction()
tv4_check_cjson_inputs()
