/*
 * deskflow-nc -- clipboard-free fork of Deskflow
 * SPDX-FileCopyrightText: (C) 2026 deskflow-nc contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 *
 * Stub replacement for src/lib/platform/MSWindowsClipboard.cpp.
 *
 * This translation unit is compiled *instead of* the real one when the build is
 * configured with -DDESKFLOW_ENABLE_CLIPBOARD=OFF (the default for this fork).
 * The class keeps its full public interface so that MSWindowsScreen.cpp still
 * compiles unchanged, but no member touches the Windows clipboard: none of
 * OpenClipboard, GetClipboardData, SetClipboardData, EmptyClipboard,
 * RegisterClipboardFormat or friends are referenced, so they do not appear in
 * the import table of the shipped binaries.
 *
 * Verify with:  dumpbin /imports deskflow-core.exe | findstr /i clipboard
 */

#include "platform/MSWindowsClipboard.h"

UINT MSWindowsClipboard::s_ownershipFormat = 0;

MSWindowsClipboard::MSWindowsClipboard(HWND window)
    : m_window(window),
      m_time(0),
      m_facade(nullptr),
      m_deleteFacade(false)
{
  // no converters are registered: there is nothing to convert
}

MSWindowsClipboard::MSWindowsClipboard(HWND window, IMSWindowsClipboardFacade &facade)
    : m_window(window),
      m_time(0),
      m_facade(&facade),
      m_deleteFacade(false)
{
  // no converters are registered: there is nothing to convert
}

MSWindowsClipboard::~MSWindowsClipboard() = default;

bool MSWindowsClipboard::emptyUnowned()
{
  return false;
}

bool MSWindowsClipboard::isOwnedByDeskflow()
{
  // this build never takes clipboard ownership
  return false;
}

bool MSWindowsClipboard::empty()
{
  return false;
}

void MSWindowsClipboard::add(Format, const std::string &)
{
  // discarded: clipboard sharing is not built in
}

bool MSWindowsClipboard::open(Time time) const
{
  m_time = time;
  // refusing to open means callers never go on to read or write data
  return false;
}

void MSWindowsClipboard::close() const
{
  // nothing was opened
}

IClipboard::Time MSWindowsClipboard::getTime() const
{
  return m_time;
}

bool MSWindowsClipboard::has(Format) const
{
  return false;
}

std::string MSWindowsClipboard::get(Format) const
{
  return {};
}

void MSWindowsClipboard::setFacade(IMSWindowsClipboardFacade &facade)
{
  m_facade = &facade;
  m_deleteFacade = false;
}

void MSWindowsClipboard::clearConverters()
{
  // no converters were ever registered
}

UINT MSWindowsClipboard::getOwnershipFormat()
{
  return s_ownershipFormat;
}
