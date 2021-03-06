#----------------------------------------------------------------------------
# Setup the project
cmake_minimum_required(VERSION 3.1.0)

project(gxbert)
set(GXBERT ON)

enable_language(CXX)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11 CACHE STRING "C++ ISO Standard")
set(CMAKE_CXX_STANDARD_REQUIRED True)

###############################################################################
if (NOT CMAKE_BUILD_TYPE)
  message(STATUS "No build type selected, default to Release")
  set(CMAKE_BUILD_TYPE "Release")
endif()
string(TOUPPER ${CMAKE_BUILD_TYPE} _build_type)

# Setting the C++ compiler to use
set("${CMAKE_CXX_COMPILER_ID}" TRUE CACHE STRING "C++ Compiler")
if (NOT GNU AND NOT Clang AND NOT AppleClang AND NOT Intel)
  message(WARNING "Unsupported compiler.")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake/modules)

include(MacroUtilities)
###############################################################################

# Configuration options

option(GXBERT_CUDA "Enable compilation for CUDA." OFF)

set(GXBERT_ARCH native sse2 sse3 ssse3 sse4.1 sse4.2 avx avx2 mic mic_avx512)

enum_option(GXBERT_VECTOR
      DOC "Vector instruction set to be used"
      TYPE STRING
      VALUES ${GXBERT_ARCH}
      CASE_INSENSITIVE
    )

if ("${BACKEND}" MATCHES "vc|VC")
  set(BACKEND "Vc")
endif()
if ("${BACKEND}" MATCHES "scalar|SCALAR")
  set(BACKEND "Scalar")
endif()

set(GXBERT_BACKEND Scalar UMESIMD Vc)

enum_option(BACKEND
            DOC "Backend infrastructure to be used"
            TYPE STRING
            VALUES ${GXBERT_BACKEND}
            CASE_SENSITIVE
           )

################################################################################

if (GXBERT_CUDA)
  set(VecCore_COMPONENTS ${VecCore_COMPONENTS} "CUDA")
  set(CUDA_SEPARABLE_COMPILATION ON CACHE BOOL "CUDA separable compilation on.")
endif()

# Determine backend

set(${BACKEND} True)
string(TOUPPER "${BACKEND}" _BACKEND_UP)
string(TOLOWER "${BACKEND}" _BACKEND_LO)

if("${BACKEND}" MATCHES "Scalar")
  set(GXBERT_SCALAR True)
  message(STATUS "Configuring with Scalar backend")
elseif ("${BACKEND}" MATCHES "Vc|UMESIMD")
  set(GXBERT_${_BACKEND_UP} True)
  set(VecCore_COMPONENTS ${VecCore_COMPONENTS} ${BACKEND})
  message(STATUS "Configuring with ${BACKEND} backend")
else()
  message(FATAL_ERROR "Unknown backend: '${BACKEND}'.")
endif()

# Find VecCore with selected components turned on (CUDA and backend)

if (GXBERT_VECTEST)
  find_package(VecCore REQUIRED COMPONENTS ${VecCore_COMPONENTS})
  include_directories(SYSTEM ${VecCore_INCLUDE_DIRS})
  set(GXBERT_DEFINITIONS "${GXBERT_DEFINITIONS} ${VecCore_DEFINITIONS}")
  set(GXBERT_DEFINITIONS "${GXBERT_DEFINITIONS} -DGXBERT_${_BACKEND_UP}")
  set(GXBERT_EXTERNAL_INCLUDES "${GXBERT_EXTERNAL_INCLUDES};${VecCore_INCLUDE_DIRS}")
  set(GXBERT_LIBRARIES_EXTERNAL ${GXBERT_LIBRARIES_EXTERNAL} ${VecCore_LIBRARIES})
endif()

################################################################################

# Set Default compiler flags for each build type

if (GNU)
  set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -Wall -fPIC")
  if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -ggdb -O0")
  else()
    set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -finline-limit=10000000 -ffast-math -ftree-vectorize")
  endif()
  set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -fabi-version=0")
  set(GXBERT_COMPILATION_FLAGS "${GXBERT_COMPILATION_FLAGS} -fabi-version=0")
elseif(Intel)
  # CMake fails to add -std option for icc, so add it manually
  if("${CMAKE_VERSION}" VERSION_LESS "3.6")
    if(CMAKE_CXX_STANDARD STREQUAL 11)
      list(APPEND CMAKE_CXX_FLAGS -std=c++11)
    elseif(CMAKE_CXX_STANDARD STREQUAL 14)
      list(APPEND CMAKE_CXX_FLAGS -std=c++14)
    else()
      message(FATAL_ERROR "Unsupported C++ standard requested")
    endif()
  endif()
  set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -Wall -fPIC -diag-disable 3438")
  if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -g -O0")
  else()
    set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -fno-alias")
  endif()
elseif(Clang OR AppleClang)
  if (APPLE)
    # prefer LLVM's standard C++ library on Mac OS X
    set (GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -stdlib=libc++")
  endif()
  set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -Wall -fPIC")
  if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS}  -ggdb -O0")
  else()
    set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -ffast-math -ftree-vectorize")
  endif()
else()
   message(WARNING "No compiler type matched")
endif()

if (CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
  set(GXBERT_NVCC_COMPILATION_FLAGS -g -G ${GXBERT_NVCC_COMPILATION_FLAGS})
endif()
if (CMAKE_BUILD_TYPE MATCHES MinSizeRel)
  set(GXBERT_NVCC_COMPILATION_FLAGS -use_fast_math ${GXBERT_NVCC_COMPILATION_FLAGS})
endif()

string(TOLOWER ${GXBERT_VECTOR} _arch_lo)
string(TOUPPER ${GXBERT_VECTOR} _arch_up)

if (${_arch_lo} MATCHES mic_avx512)
  if (Intel)
    set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -xMIC_AVX512")
    set(GXBERT_COMPILATION_FLAGS "${GXBERT_COMPILATION_FLAGS} -xMIC_AVX512")
  else()
    set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -march=knl")
    set(GXBERT_COMPILATION_FLAGS "${GXBERT_COMPILATION_FLAGS} -march=knl")
  endif()
elseif (${_arch_lo} MATCHES mic)
  if (Intel)
    set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -m${_arch_lo}")
    set(GXBERT_COMPILATION_FLAGS "${GXBERT_COMPILATION_FLAGS} -m${_arch_lo}")
    set(MIC TRUE)
  else()
   message(FATAL_ERROR "MIC native or offload compilation requires Intel Compiler.")
  endif()
elseif (${_arch_lo} MATCHES native)
  if (Intel)
    set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -xHost")
    set(GXBERT_COMPILATION_FLAGS "${GXBERT_COMPILATION_FLAGS} -xHost")
  else()
    set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -march=${_arch_lo}")
    set(GXBERT_COMPILATION_FLAGS "${GXBERT_COMPILATION_FLAGS} -march=${_arch_lo}")
  endif()
else()
  if (Intel)
    if (${_arch_lo} MATCHES avx2)
      set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -xCORE-${_arch_up}")
      set(GXBERT_COMPILATION_FLAGS "${GXBERT_COMPILATION_FLAGS} -xCORE-${_arch_up}")
    else()
      set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -x${_arch_up}")
      set(GXBERT_COMPILATION_FLAGS "${GXBERT_COMPILATION_FLAGS} -x${_arch_up}")
    endif()
  else()
    set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -m${_arch_lo}")
    set(GXBERT_COMPILATION_FLAGS "${GXBERT_COMPILATION_FLAGS} -m${_arch_lo}")
  endif()
endif()
message(STATUS "Compiling for ${_arch_up} SIMD architecture")

################################################################################

# Set C++ compiler flags. Will also be forwarded by CUDA when compiling C++.
if (GXBERT_CUDA)
  set(GXBERT_COMPILATION_FLAGS "${GXBERT_COMPILATION_FLAGS} -DGXBERT_CUDA")

  set(CUDA_ARCH 30 CACHE STRING "CUDA device architecture.")
  set(CUDA_ARCH "-arch=sm_${CUDA_ARCH}")
  set(GXBERT_DEFINITIONS "${GXBERT_DEFINITIONS} -DGXBERT_ENABLE_CUDA")
  set(GXBERT_NVCC_COMPILATION_FLAGS ${GXBERT_NVCC_COMPILATION_FLAGS}
      -std=c++11
      -Xcompiler -Wno-unused-function
      -Xcudafe "--diag_suppress=code_is_unreachable"
      -Xcudafe "--diag_suppress=initialization_not_reachable")
  set(GXBERT_NVCC_CONFIGS
        DEBUG -g -G
        RELEASE  -O3 -use_fast_math
        RELWITHDEBINFO -g -G -O3 -use_fast_math
        MINSIZEREL -use_fast_math )
  if (NOT (CMAKE_VERSION LESS 3.3))
     cmake_policy(SET CMP0058 OLD)
  endif()
endif()

if (Clang OR AppleClang)
  set(GXBERT_LIBRARIES_EXTERNAL ${GXBERT_LIBRARIES_EXTERNAL} dl)
endif()

# Enable pretty diagnostics coloring if gcc 4.9+ is being used
if (GNU)
  # gcc needs -ldl to be added explicitly
  set(GXBERT_LIBRARIES_EXTERNAL ${GXBERT_LIBRARIES_EXTERNAL} dl)
  set(GXBERT_ERROR_LIMIT 20 CACHE STRING
      "Limit number of errors output by diagnostics.")
  set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -fmax-errors=${GXBERT_ERROR_LIMIT}")
  execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
                  OUTPUT_VARIABLE GCC_VERSION)
  if (GCC_VERSION VERSION_GREATER 4.9 OR GCC_VERSION VERSION_EQUAL 4.9)
    set(GXBERT_CXX_FLAGS "${GXBERT_CXX_FLAGS} -fdiagnostics-color=auto")
  endif()
endif()

################################################################################

# see if we have CCACHE ( to avoid recompilation on branch switches )
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  message(STATUS "found ccache")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

if (GXBERT_CUDA)
  find_package(CUDA REQUIRED)
  set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS} ${GXBERT_NVCC_COMPILATION_FLAGS})
endif()

# Pass flags to compilers
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${_build_type}} ${GXBERT_CXX_FLAGS} ${GXBERT_DEFINITIONS} ${GXBERT_COMPILATION_FLAGS}")
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

message(STATUS "Compiling with C++ flags: ${CMAKE_CXX_FLAGS}")
if (GXBERT_CUDA)
  message(STATUS "Compiling with NVCC flags: ${CUDA_NVCC_FLAGS}")
endif()

################################################################################

include_directories(${PROJECT_SOURCE_DIR} include 
                                          base/external 
                                          base/clhep/include )

set(SRC_CPP_RELATIVE
 src/bindingEnergyAsymptotic.cc
  src/bindingEnergy.cc
  src/G4Analyser.cc
  src/G4BigBanger.cc
  src/G4CascadeChannel.cc
  src/G4CascadeChannelTables.cc
  src/G4CascadeCheckBalance.cc
  src/G4CascadeCoalescence.cc
  src/G4CascadeColliderBase.cc
  src/G4CascadeDeexcitation.cc
  src/G4CascadeDeexciteBase.cc
  src/G4CascadeFinalStateAlgorithm.cc
  src/G4CascadeFinalStateGenerator.cc
  src/G4CascadeHistory.cc
  src/G4CascadeKminusNChannel.cc
  src/G4CascadeKminusPChannel.cc
  src/G4CascadeKplusNChannel.cc
  src/G4CascadeKplusPChannel.cc
  src/G4CascadeKzeroBarNChannel.cc
  src/G4CascadeKzeroBarPChannel.cc
  src/G4CascadeKzeroNChannel.cc
  src/G4CascadeKzeroPChannel.cc
  src/G4CascadeLambdaNChannel.cc
  src/G4CascadeLambdaPChannel.cc
  src/G4CascadeMuMinusPChannel.cc
  src/G4CascadeNNChannel.cc
  src/G4CascadeNPChannel.cc
  src/G4CascadeOmegaMinusNChannel.cc
  src/G4CascadeOmegaMinusPChannel.cc
  src/G4CascadeParameters.cc
  src/G4CascadePPChannel.cc
  src/G4CascadeRecoilMaker.cc
  src/G4CascadeSigmaMinusNChannel.cc
  src/G4CascadeSigmaMinusPChannel.cc
  src/G4CascadeSigmaPlusNChannel.cc
  src/G4CascadeSigmaPlusPChannel.cc
  src/G4CascadeSigmaZeroNChannel.cc
  src/G4CascadeSigmaZeroPChannel.cc
  src/G4CascadeT11pizNChannel.cc
  src/G4CascadeT1GamNChannel.cc
  src/G4CascadeT31piNChannel.cc
  src/G4CascadeT33piNChannel.cc
  src/G4CascadeXiMinusNChannel.cc
  src/G4CascadeXiMinusPChannel.cc
  src/G4CascadeXiZeroNChannel.cc
  src/G4CascadeXiZeroPChannel.cc
  src/G4CascadParticle.cc
  src/G4CollisionOutput.cc
  src/G4Dineutron.cc
  src/G4Diproton.cc
  src/G4ElementaryParticleCollider.cc
  src/G4EquilibriumEvaporator.cc
  src/G4EvaporationInuclCollider.cc
  src/G4ExitonConfiguration.cc
  src/G4FissionConfiguration.cc
  src/G4Fissioner.cc
  src/G4FissionStore.cc
  src/G4GammaNuclAngDst.cc
  src/G4GamP2NPipAngDst.cc
  src/G4GamP2PPi0AngDst.cc
  src/G4GDecay3.cc
  src/G4HadNElastic1AngDst.cc
  src/G4HadNElastic2AngDst.cc
  src/G4HadNucl3BodyAngDst.cc
  src/G4HadNucl3BodyMomDst.cc
  src/G4HadNucl4BodyMomDst.cc
  src/G4HyperonSampler.cc
  src/G4InteractionCase.cc
  src/G4IntraNucleiCascader.cc
  src/G4InuclCollider.cc
  src/G4InuclElementaryParticle.cc
  src/G4InuclNuclei.cc
  src/G4InuclParamAngDst.cc
  src/G4InuclParamMomDst.cc
  src/G4InuclParticle.cc
  src/G4InuclParticleNames.cc
  src/G4InuclSpecialFunctions.cc
  src/G4KaonHypSampler.cc
  src/G4KaonSampler.cc
  src/G4LorentzConvertor.cc
  src/G4MultiBodyMomentumDist.cc
  src/G4NonEquilibriumEvaporator.cc
  src/G4NP2NPAngDst.cc
  src/G4NucleiModel.cc
  src/G4NuclNucl3BodyAngDst.cc
  src/G4NuclNucl3BodyMomDst.cc
  src/G4NuclNucl4BodyMomDst.cc
  src/G4NuclNuclAngDst.cc
  src/G4NuclWatcher.cc
  src/G4Pi0P2Pi0PAngDst.cc
  src/G4PimP2Pi0NAngDst.cc
  src/G4PimP2PimPAngDst.cc
  src/G4PiNInelasticAngDst.cc
  src/G4PionNucSampler.cc
  src/G4PipP2PipPAngDst.cc
  src/G4PP2PPAngDst.cc
  src/G4TwoBodyAngularDist.cc
  src/G4UnboundPN.cc
  src/G4VCascadeCollider.cc
  src/G4VCascadeDeexcitation.cc
  src/G4VMultiBodyMomDst.cc
  src/G4VTwoBodyAngDst.cc
  src/G4WatcherGun.cc
  src/GXAllocator.cc
  src/GXAllocatorList.cc
  src/GXAllocatorPool.cc
  src/GXAlpha.cc
  src/GXAntiKaonZero.cc
  src/GXCascadeInterface.cc
  src/GXDecayProducts.cc
  src/GXDecayTable.cc
  src/GXDeuteron.cc
  src/GXDynamicParticle.cc
  src/GXElectronOccupancy.cc
  src/GXException.cc
  src/GXFragment.cc
  src/GXGamma.cc
  src/GXHadDecayGenerator.cc
  src/GXHadPhaseSpaceGenbod.cc
  src/GXHadPhaseSpaceKopylov.cc
  src/GXHadPhaseSpaceNBodyAsai.cc
  src/GXHe3.cc
  src/GXIons.cc
  src/GXIonTable.cc
  src/GXios.cc
  src/GXKaonMinus.cc
  src/GXKaonPlus.cc
  src/GXKaonZero.cc
  src/GXKaonZeroLong.cc
  src/GXKaonZeroShort.cc
  src/GXLambda.cc
  src/GXNeutron.cc
  src/GXNucleiProperties.cc
  src/GXNucleiPropertiesTableAME12.cc
  src/GXNucleiPropertiesTheoreticalTableA.cc
  src/GXNucleiPropertiesTheoreticalTableB.cc
  src/GXOmegaMinus.cc
  src/GXParticleDefinition.cc
  src/GXParticleTable.cc
  src/GXPDGCodeChecker.cc
  src/GXPhaseSpaceDecayChannel.cc
  src/GXPionMinus.cc
  src/GXPionPlus.cc
  src/GXPionZero.cc
  src/GXPow.cc
  src/GXProton.cc
  src/GXSigmaMinus.cc
  src/GXSigmaPlus.cc
  src/GXSigmaZero.cc
  src/GXTriton.cc
  src/GXVDecayChannel.cc
  src/GXVHadDecayAlgorithm.cc
  src/GXVHadPhaseSpaceAlgorithm.cc
  src/GXVShortLivedParticle.cc
  src/GXXiMinus.cc
  src/GXXiZero.cc
  src/nucleiLevelDensity.cc
  src/paraMaker.cc

  base/clhep/src/DoubConv.cc
  base/clhep/src/LorentzRotation.cc
  base/clhep/src/LorentzVector.cc
  base/clhep/src/LorentzVectorL.cc
  base/clhep/src/Rotation.cc
  base/clhep/src/RotationInterfaces.cc
  base/clhep/src/setStdMath.cc
  base/clhep/src/setSystemOfUnits.cc
  base/clhep/src/SpaceVector.cc
  base/clhep/src/ThreeVector.cc
  base/clhep/src/ThreeVectorR.cc
  base/clhep/src/ZMinput.cc

  base/clhep/src/DualRand.cc
  base/clhep/src/EngineFactory.cc
  base/clhep/src/engineIDulong.cc
  base/clhep/src/JamesRandom.cc
  base/clhep/src/mixmax.cc
  base/clhep/src/MixMaxRng.cc
  base/clhep/src/MTwistEngine.cc
  base/clhep/src/NonRandomEngine.cc
  base/clhep/src/RandFlat.cc
  base/clhep/src/RandGauss.cc
  base/clhep/src/RandGeneral.cc
  base/clhep/src/Random.cc
  base/clhep/src/RandomEngine.cc
  base/clhep/src/RanecuEngine.cc
  base/clhep/src/Ranlux64Engine.cc
  base/clhep/src/RanluxEngine.cc
  base/clhep/src/RanshiEngine.cc
  base/clhep/src/StaticRandomStates.cc

#  base/clhep/src/AxisAngle.cc
#  base/clhep/src/BasicVector3D.cc
#  base/clhep/src/Boost.cc
#  base/clhep/src/BoostX.cc
#  base/clhep/src/BoostY.cc
#  base/clhep/src/BoostZ.cc
#  base/clhep/src/erfQ.cc
#  base/clhep/src/EulerAngles.cc
#  base/clhep/src/Evaluator.cc
#  base/clhep/src/flatToGaussian.cc
#  base/clhep/src/gammln.cc
#  base/clhep/src/LorentzRotationC.cc
#  base/clhep/src/LorentzRotationD.cc
#  base/clhep/src/LorentzVectorB.cc
#  base/clhep/src/LorentzVectorC.cc
#  base/clhep/src/LorentzVectorK.cc
#  base/clhep/src/LorentzVectorR.cc
#  base/clhep/src/Normal3D.cc
#  base/clhep/src/Plane3D.cc
#  base/clhep/src/Point3D.cc
#  base/clhep/src/RandBinomial.cc
#  base/clhep/src/RandBit.cc
#  base/clhep/src/RandBreitWigner.cc
#  base/clhep/src/RandChiSquare.cc
#  base/clhep/src/RandExponential.cc
#  base/clhep/src/RandExpZiggurat.cc
#  base/clhep/src/RandGamma.cc
#  base/clhep/src/RandGaussQ.cc
#  base/clhep/src/RandGaussZiggurat.cc
#  base/clhep/src/RandLandau.cc
#  base/clhep/src/RandPoisson.cc
#  base/clhep/src/RandPoissonQ.cc
#  base/clhep/src/RandStudentT.cc
#  base/clhep/src/RotationA.cc
#  base/clhep/src/RotationC.cc
#  base/clhep/src/RotationE.cc
#  base/clhep/src/RotationIO.cc
#  base/clhep/src/RotationL.cc
#  base/clhep/src/RotationP.cc
#  base/clhep/src/RotationX.cc
#  base/clhep/src/RotationY.cc
#  base/clhep/src/RotationZ.cc
#  base/clhep/src/SpaceVectorD.cc
#  base/clhep/src/SpaceVectorP.cc
#  base/clhep/src/SpaceVectorR.cc
#  base/clhep/src/Transform3D.cc
#  base/clhep/src/TwoVector.cc
#  base/clhep/src/Vector3D.cc
)

if (GXBERT_CUDA)
#-------------------------------------------------------------------------------
# Copy source files to .cu files in order to compile them with NVCC
#
  foreach(SRC_FILE ${SRC_CPP})
    get_filename_component(SRC_FILENAME ${SRC_FILE} NAME_WE)

    ADD_CUSTOM_COMMAND(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cuda_src/${SRC_FILENAME}.cu
      COMMAND ${CMAKE_COMMAND} -E copy ${SRC_FILE}
      ${CMAKE_CURRENT_BINARY_DIR}/cuda_src/${SRC_FILENAME}.cu
      DEPENDS ${SRC_FILE}
      )
    
    set(
      SRC_CUDA ${SRC_CUDA}
      ${CMAKE_CURRENT_BINARY_DIR}/cuda_src/${SRC_FILENAME}.cu
      )
  endforeach()

#  set(SRC_CUDA ${SRC_CUDA}
#  add cuda tests
#  )
endif()

foreach(SRC ${SRC_CPP_RELATIVE})
  set(SRC_CPP ${SRC_CPP} ${CMAKE_CURRENT_SOURCE_DIR}/${SRC})
endforeach()

set(GXBERT_SRCS
  ${GXBERT_SRCS}
  ${GXBERT_CUDA_SRCS}
)

foreach(SRC ${GXBERT_SRCS})
  set(SRC_CPP ${SRC_CPP} ${CMAKE_SOURCE_DIR}/${SRC})
endforeach()

################################################################################
# Build libraries

add_library(gxbert ${SRC_CPP})
target_link_libraries(gxbert ${GXBERT_LIBRARIES}
                      ${GXBERT_LIBRARIES_EXTERNAL})

set(GXBERT_LIBRARIES ${GXBERT_LIBRARIES} gxbert)

if (APPLE)
   set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
else()
  set(GXBERT_LIBRARIES_EXTERNAL ${GXBERT_LIBRARIES_EXTERNAL} rt)
endif()

if (BUILD_SHARED_LIBS)
    set(GXBERT_LIBNAME "libgxbert${CMAKE_SHARED_LIBRARY_SUFFIX}")
else()
    set(GXBERT_LIBNAME "libgxbert${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()

# build the CUDA version of the library
if (GXBERT_CUDA)
  cuda_add_library(
    gxbertcuda
    ${SRC_CUDA}
    SHARED
    OPTIONS ${CUDA_ARCH}
    ${GXBERT_NVCC_CONFIGS}
  )
  target_link_libraries(gxbertcuda ${GXBERT_LIBRARIES}
                        ${GXBERT_LIBRARIES_EXTERNAL})
  set(GXBERT_LIBRARIES ${GXBERT_LIBRARIES} gxbertcuda)
endif()

################################################################################
# Define executables
set(TEST_EXECUTABLES
  ${PROJECT_SOURCE_DIR}/test/gxbertTest.cc
)

if (GXBERT_VECTEST)
  set(TEST_EXECUTABLES ${TEST_EXECUTABLES}
    ${PROJECT_SOURCE_DIR}/test/gvbertTest.cc
  ) 
endif()

# Build executables
set(GXBERT_EXECUTABLES)
#
macro(build_executables EXECUTABLES)
  foreach(EXECUTABLE ${EXECUTABLES})
    get_filename_component(TARGET_NAME ${EXECUTABLE} NAME_WE)
    add_executable(${TARGET_NAME} ${EXECUTABLE})
    target_link_libraries(${TARGET_NAME} ${GXBERT_LIBRARIES}
                          ${GXBERT_LIBRARIES_EXTERNAL})
    set(GXBERT_EXECUTABLES ${GXBERT_EXECUTABLES} ${TARGET_NAME})
  endforeach()
endmacro()

build_executables("${TEST_EXECUTABLES}")
if (GXBERT_CUDA)
  build_executables("${TEST_EXECUTABLES_CUDA}")
endif()

################################################################################
# Installation - do not install yet
