function(llcl_enable_sanitizers target)
  if(NOT LLCL_ENABLE_SANITIZERS)
    return()
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
      ${target}
      PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer
    )
    target_link_options(
      ${target}
      PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer
    )
  else()
    message(WARNING "LLCL sanitizers are only configured for GCC and Clang")
  endif()
endfunction()
