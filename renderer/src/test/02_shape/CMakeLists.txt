Ubpa_GetTargetName(dx12fg ${PROJECT_SOURCE_DIR}/src/dx12fg)
Ubpa_AddTarget(
  TEST
  MODE EXE
  LIB
    ${dx12fg}
)
