file(GLOB_RECURSE LAMUSICA_MARKDOWN_FILES "${CMAKE_CURRENT_LIST_DIR}/../*.md")

set(LAMUSICA_MARKDOWN_ERRORS)

foreach(markdown_file IN LISTS LAMUSICA_MARKDOWN_FILES)
  if(markdown_file MATCHES "/(\\.git|build|_CPack_Packages)/")
    continue()
  endif()

  file(STRINGS "${markdown_file}" markdown_lines)
  set(line_number 0)

  foreach(line IN LISTS markdown_lines)
    math(EXPR line_number "${line_number} + 1")

    if(line MATCHES "\t")
      list(APPEND LAMUSICA_MARKDOWN_ERRORS
           "${markdown_file}:${line_number}: contains a tab character")
    endif()

  endforeach()
endforeach()

if(LAMUSICA_MARKDOWN_ERRORS)
  list(JOIN LAMUSICA_MARKDOWN_ERRORS "\n" LAMUSICA_MARKDOWN_ERROR_TEXT)
  message(FATAL_ERROR "Markdown lint failed:\n${LAMUSICA_MARKDOWN_ERROR_TEXT}")
endif()
