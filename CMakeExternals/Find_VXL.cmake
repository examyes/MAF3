  SET(VXL_DIR "${MAF_EXTERNAL_BUILD_DIR}/Build/VXL")
  FIND_PACKAGE(VXL REQUIRED)
  include(${VXL_DIR}/VXLConfig.cmake)
  INCLUDE(${VXL_DIR}/UseVXL.cmake)
