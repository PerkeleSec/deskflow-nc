/*
 * deskflow-nc -- clipboard-free fork of Deskflow
 * SPDX-FileCopyrightText: (C) 2026 deskflow-nc contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 *
 * Stub replacement for src/lib/platform/OSXClipboard.cpp.
 *
 * This translation unit is compiled *instead of* the real one when the build is
 * configured with -DDESKFLOW_ENABLE_CLIPBOARD=OFF (the default for this fork).
 * The class keeps its full public interface so that OSXScreen.mm still compiles
 * unchanged (it holds an OSXClipboard by value), but no member touches the
 * macOS pasteboard: PasteboardCreate, PasteboardCopyItemFlavorData,
 * PasteboardPutItemFlavor, PasteboardSynchronize and friends are never
 * referenced, so they do not appear in the undefined-symbol table of the
 * shipped binaries.
 *
 * Verify with:  nm -u deskflow-core | grep -i pasteboard
 */

#include "platform/OSXClipboard.h"

OSXClipboard::OSXClipboard()
    : m_time(0),
      m_pboard(nullptr)
{
  // no pasteboard is created and no converters are registered
}

OSXClipboard::~OSXClipboard() = default;

bool OSXClipboard::isOwnedByDeskflow()
{
  // this build never takes pasteboard ownership
  return false;
}

bool OSXClipboard::empty()
{
  return false;
}

void OSXClipboard::add(Format, const std::string &)
{
  // discarded: clipboard sharing is not built in
}

bool OSXClipboard::open(Time time) const
{
  m_time = time;
  // refusing to open means callers never go on to read or write data
  return false;
}

void OSXClipboard::close() const
{
  // nothing was opened
}

IClipboard::Time OSXClipboard::getTime() const
{
  return m_time;
}

bool OSXClipboard::has(Format) const
{
  return false;
}

std::string OSXClipboard::get(Format) const
{
  return {};
}

bool OSXClipboard::synchronize()
{
  // the pasteboard is never inspected, so it never "changes"
  return false;
}

void OSXClipboard::clearConverters()
{
  // no converters were ever registered
}
