# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ..
  ../blenkernel
  ../blenlib
  ../blentranslation
  ../bmesh
  ../depsgraph
  ../editors/include
  ../gpu
  ../imbuf
  ../makesdna
  ../makesrna
  ../nodes
  ../render
  ../windowmanager
  ../../blenloader
  ../../imbuf
  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  intern/qfea_material.c
  intern/qfea_physics_fdtd.c
  intern/qfea_physics_particle.c
  intern/qfea_physics_quantum.c
  intern/qfea_tensor.c
  intern/qfea_voxel.c

  QFEA_compute.h
  QFEA_draw.h
  QFEA_io.h
  QFEA_llm.h
  QFEA_material.h
  QFEA_node.h
  QFEA_physics.h
  QFEA_preset.h
  QFEA_tensor.h
  QFEA_timeline.h
  QFEA_voxel.h
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  bf_blenkernel
)

# Optional dependencies
if(WITH_OPENVDB)
  add_definitions(-DWITH_OPENVDB)
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENVDB_LIBRARIES}
  )
endif()

if(WITH_CUDA)
  add_definitions(-DWITH_CUDA)
  list(APPEND INC_SYS
    ${CUDA_INCLUDE_DIRS}
  )
endif()

if(WITH_OPENCL)
  add_definitions(-DWITH_OPENCL)
  list(APPEND INC_SYS
    ${OPENCL_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENCL_LIBRARIES}
  )
endif()

if(WITH_METAL)
  add_definitions(-DWITH_METAL)
endif()

if(WITH_HDF5)
  add_definitions(-DWITH_HDF5)
  list(APPEND INC_SYS
    ${HDF5_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${HDF5_LIBRARIES}
  )
endif()

blender_add_lib(bf_qfea "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# CUDA kernel compilation (if enabled)
if(WITH_CUDA)
  set(CUDA_SRC
    intern/qfea_kernels_cuda.cu
  )

  # CUDA compilation would be configured here
  # cuda_add_library(bf_qfea_cuda ${CUDA_SRC})
  # target_link_libraries(bf_qfea bf_qfea_cuda)
endif()

# OpenCL kernel compilation (if enabled)
if(WITH_OPENCL)
  set(OPENCL_SRC
    # intern/qfea_kernels_opencl.cl
  )

  # OpenCL kernels embedded as strings
  # Would be processed here
endif()

# Metal shader compilation (if enabled)
if(WITH_METAL AND APPLE)
  set(METAL_SRC
    # intern/qfea_kernels_metal.metal
  )

  # Metal shader compilation would be configured here
endif()
