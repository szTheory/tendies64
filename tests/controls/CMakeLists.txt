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

# Live suite controls (label "controls"): the whole matrix is sealed into a
# suite verified against INVENTORY, and the suite-level negative controls run
# on that live output. controls.inventory pins the matrix to INVENTORY.
foreach(t64_pair "suite:suite" "suite-dropped:dropped" "suite-truncated:truncated"
        "inventory-drift:drift" "inventory:inventory")
  string(REPLACE ":" ";" t64_pair_parts "${t64_pair}")
  list(GET t64_pair_parts 0 t64_test_name)
  list(GET t64_pair_parts 1 t64_mode)
  add_test(NAME controls.${t64_test_name}
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/run-suite.sh
            --bin-dir $<TARGET_FILE_DIR:t64-supervisor> --mode ${t64_mode})
  set_tests_properties(controls.${t64_test_name} PROPERTIES LABELS controls)
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/INVENTORY")
