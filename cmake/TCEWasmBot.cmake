# ET Legacy uses native Wasm i64 imports for time and C++ runtime calls.
target_link_options(omnibot-et PRIVATE -sWASM_BIGINT=1)
target_link_libraries(omnibot-et "-Wl,--no-whole-archive"
  c++ c++abi "${TCE_WASM_BUILTINS_LIBRARY}" "-Wl,--whole-archive")
