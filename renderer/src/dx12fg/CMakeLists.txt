Ubpa_GetTargetName(core ${PROJECT_SOURCE_DIR}/src/core)

Ubpa_AddTarget(
  TEST
  MODE STATIC
  LIB
    Ubpa::UFG_core
  LIB_INTERFACE
    ${core}
)
