# SPDX-License-Identifier: AGPL-3.0-only
set(QUPATHXX_OME_MASK_ZLIB_ARCHIVE "" CACHE FILEPATH "Admitted offline zlib 1.3.2 archive")
set(QUPATHXX_OME_MASK_BZIP2_ARCHIVE "" CACHE FILEPATH "Admitted offline bzip2 1.0.8 archive")

function(qpx_ome_mask_codec target archive digest stem header)
  if(NOT EXISTS "${archive}")
    message(FATAL_ERROR "Provide the admitted ${stem} source archive for the OME ROI module")
  endif()
  file(SHA256 "${archive}" actual)
  if(NOT actual STREQUAL digest)
    message(FATAL_ERROR "Unexpected source archive hash for ${stem}")
  endif()
  set(destination "${CMAKE_CURRENT_BINARY_DIR}/mask-codecs/${digest}")
  set(sources "${destination}/${stem}")
  if(NOT EXISTS "${sources}/${header}")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${destination}")
  endif()
  set(units ${ARGN})
  list(TRANSFORM units PREPEND "${sources}/")
  add_library(${target} STATIC ${units})
  target_include_directories(${target} PUBLIC "${sources}")
  target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
  set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
  target_link_libraries(qpx_ome_roi_plugin PRIVATE ${target})
endfunction()

qpx_ome_mask_codec(qpx_ome_mask_zlib "${QUPATHXX_OME_MASK_ZLIB_ARCHIVE}"
  bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16 zlib-1.3.2 zlib.h
  adler32.c crc32.c inffast.c inflate.c inftrees.c zutil.c)
target_compile_definitions(qpx_ome_mask_zlib PUBLIC Z_PREFIX)
qpx_ome_mask_codec(qpx_ome_mask_bzip2 "${QUPATHXX_OME_MASK_BZIP2_ARCHIVE}"
  ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269 bzip2-1.0.8 bzlib.h
  blocksort.c huffman.c crctable.c randtable.c compress.c decompress.c bzlib.c)
target_compile_definitions(qpx_ome_roi_plugin PRIVATE NOMINMAX)
