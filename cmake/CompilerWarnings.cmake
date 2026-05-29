function(lamusica_configure_target target_name)
  target_compile_features(${target_name} PUBLIC cxx_std_23)

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    target_compile_options(
      ${target_name}
      PRIVATE -Wall
              -Wextra
              -Wpedantic
              -Wconversion
              -Wsign-conversion
              -Wshadow
              -Wnon-virtual-dtor)
  elseif(MSVC)
    target_compile_options(${target_name} PRIVATE /W4 /permissive-)
  endif()

  if(LAMUSICA_ENABLE_ASAN AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    target_compile_options(${target_name} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(${target_name} PRIVATE -fsanitize=address)
  endif()

  if(LAMUSICA_ENABLE_TSAN AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    target_compile_options(${target_name} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(${target_name} PRIVATE -fsanitize=thread)
  endif()

  if(LAMUSICA_ENABLE_PROFILING AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    target_compile_options(${target_name} PRIVATE -g)
  endif()
endfunction()
