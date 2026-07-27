# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

# deskflow-nc: MSWindowsClipboard/OSXClipboard are replaced by stubs unless
# clipboard sharing is enabled, so their behavioural tests only make sense then.
# The platform-independent Clipboard/ClipboardChunk tests still run either way.
if (WIN32)
  if(DESKFLOW_ENABLE_CLIPBOARD)
    create_test(
      NAME MSWindowsClipboardTests
      DEPENDS platform
      LIBS base arch
      SOURCE MSWindowsClipboardTests.cpp
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
    )
  endif()
elseif(APPLE)
  if(DESKFLOW_ENABLE_CLIPBOARD)
    create_test(
      NAME OSXClipboardTests
      DEPENDS platform
      LIBS base arch
      SOURCE OSXClipboardTests.cpp
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
    )
  endif()
  create_test(
    NAME OSXKeyStateTests
    DEPENDS platform
    LIBS base arch
    SOURCE OSXKeyStateTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
elseif(UNIX)
  if (BUILD_X11_SUPPORT)
    create_test(
      NAME XWindowsClipboardTests
      DEPENDS platform
      LIBS base arch
      SOURCE XWindowsClipboardTests.cpp
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
    )
  endif()
endif()
