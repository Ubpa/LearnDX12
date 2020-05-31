Ubpa_GetTargetName(common ${PROJECT_SOURCE_DIR}/src/common)

Ubpa_AddTarget(
  TEST
  MODE STATIC
  LIB_INTERFACE
    ${common}
)
