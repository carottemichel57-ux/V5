cmake_minimum_required(VERSION 3.5)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "VITASDK n'est pas défini")
  endif()
endif()

project(vitacraft C)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O2 -std=gnu99")

add_executable(vitacraft main.c)

# Fichiers (sauvegarde des reglages + codes SD) : on ne lie que les stubs presents dans ton VitaSDK
set(EXTRA_STUBS "")
foreach(lib SceIofilemgr_stub)
  if(EXISTS "$ENV{VITASDK}/arm-vita-eabi/lib/lib${lib}.a")
    list(APPEND EXTRA_STUBS ${lib})
  endif()
endforeach()

target_link_libraries(vitacraft
  taihen_stub
  SceLibKernel_stub
  SceProcessmgr_stub
  SceDisplay_stub
  SceCtrl_stub
  ${EXTRA_STUBS}
)

set_target_properties(vitacraft PROPERTIES LINK_FLAGS "-nostartfiles")

# exports.yml (ou exports.yaml si le telephone a change l'extension)
if(EXISTS "${CMAKE_SOURCE_DIR}/exports.yml")
  set(EXPORTS_FILE exports.yml)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/exports.yaml")
  set(EXPORTS_FILE exports.yaml)
else()
  message(FATAL_ERROR "exports.yml introuvable")
endif()

vita_create_self(vitacraft.suprx vitacraft CONFIG ${EXPORTS_FILE} UNSAFE)
