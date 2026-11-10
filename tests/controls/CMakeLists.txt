# One CTest test per row of matrix.tsv (label "controls"). Scripts and goldens
# are directory-owned corpora (CONTEXT A-6).
set(T64_MATRIX "${CMAKE_CURRENT_SOURCE_DIR}/matrix.tsv")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${T64_MATRIX}")
file(STRINGS "${T64_MATRIX}" T64_MATRIX_LINES)
foreach(t64_line IN LISTS T64_MATRIX_LINES)
  if(t64_line STREQUAL "" OR t64_line MATCHES "^#")
    continue()
  endif()
  string(REPLACE "\t" ";" t64_cols "${t64_line}")
  list(GET t64_cols 0 t64_id)
  add_test(NAME controls.${t64_id}
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/run-case.sh
            --bin-dir $<TARGET_FILE_DIR:t64-supervisor> --case ${t64_id})
  set_tests_properties(controls.${t64_id} PROPERTIES LABELS controls)
endforeach()
