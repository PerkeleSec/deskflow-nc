/*
 * deskflow-nc -- clipboard-free fork of Deskflow
 * SPDX-FileCopyrightText: (C) 2026 deskflow-nc contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 *
 * Stub replacement for src/lib/platform/XWindowsClipboard.cpp.
 *
 * This translation unit is compiled *instead of* the real one when the build is
 * configured with -DDESKFLOW_ENABLE_CLIPBOARD=OFF (the default for this fork).
 * The class keeps its full public interface so that XWindowsScreen.cpp compiles
 * unchanged, but no member touches an X selection: XSetSelectionOwner,
 * XGetSelectionOwner, XConvertSelection and the ICCCM/Motif machinery are never
 * referenced, so they do not appear among the binary's undefined symbols.
 *
 * XWindowsScreen never constructs one of these in a clipboard-free build --
 * every m_clipboard[] entry stays null -- so in practice none of this runs. It
 * exists so the class remains linkable and so that a future call site added
 * during a rebase fails safe rather than reaching X11.
 *
 * Verify with:  nm -u deskflow-core | grep -iE 'selection|clipboard'
 */

#include "platform/XWindowsClipboard.h"

XWindowsClipboard::XWindowsClipboard(Display *display, Window window, ClipboardID id)
    : m_display(display),
      m_window(window),
      m_id(id),
      m_selection(None),
      m_motif(false),
      m_checkCache(false),
      m_cached(false),
      m_cacheTime(0)
{
  for (auto &added : m_added) {
    added = false;
  }
  // no selection atoms are interned and no converters are registered
}

XWindowsClipboard::~XWindowsClipboard() = default;

void XWindowsClipboard::lost(Time time)
{
  m_owner = false;
  m_timeLost = time;
}

void XWindowsClipboard::addRequest(Window, Window, Atom, ::Time, Atom)
{
  // never owned, so there is nothing to serve and nothing to refuse
}

bool XWindowsClipboard::processRequest(Window, ::Time, Atom)
{
  return false;
}

bool XWindowsClipboard::destroyRequest(Window)
{
  return false;
}

Window XWindowsClipboard::getWindow() const
{
  return m_window;
}

Atom XWindowsClipboard::getSelection() const
{
  return m_selection;
}

bool XWindowsClipboard::empty()
{
  return false;
}

void XWindowsClipboard::add(Format, const std::string &)
{
  // discarded: clipboard sharing is not built in
}

bool XWindowsClipboard::open(Time time) const
{
  m_time = time;
  // refusing to open means callers never go on to read or write data
  return false;
}

void XWindowsClipboard::close() const
{
  // nothing was opened
}

IClipboard::Time XWindowsClipboard::getTime() const
{
  return m_timeOwned;
}

bool XWindowsClipboard::has(Format) const
{
  return false;
}

std::string XWindowsClipboard::get(Format) const
{
  return {};
}

void XWindowsClipboard::clearConverters()
{
  // no converters were ever registered
}
