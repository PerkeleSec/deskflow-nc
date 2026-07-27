/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AboutDialog.h"
#include "ui_AboutDialog.h"

#include "common/Constants.h"
#include "common/Settings.h"
#include "common/VersionInfo.h"

#ifndef DESKFLOW_NO_CLIPBOARD
#include <QClipboard>
#endif

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent), ui{std::make_unique<Ui::AboutDialog>()}
{
  ui->setupUi(this);

  const int px = (fontMetrics().height() * 6);
  const QSize pixmapSize(px, px);
  ui->lblIcon->setFixedSize(pixmapSize);

  ui->lblIcon->setPixmap(QPixmap(QIcon::fromTheme(kRevFqdnName).pixmap(QSize().scaled(pixmapSize, Qt::KeepAspectRatio)))
  );

#ifdef DESKFLOW_NO_CLIPBOARD
  // "copy version info" is the one place the GUI would write to the local
  // clipboard. It is dropped so that no build artifact references QClipboard
  // at all and the symbol checks in doc/no-clipboard.md stay clean.
  ui->btnCopyVersion->setVisible(false);
#else
  ui->btnCopyVersion->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::EditCopy));
  connect(ui->btnCopyVersion, &QPushButton::clicked, this, &AboutDialog::copyVersionText);
#endif

  ui->lblVersion->setText(kDisplayVersion);
  ui->lblDescription->setText(kAppDescription);
  ui->lblCopyright->setText(kCopyright);

  // Use non-breaking space in each awesome dev name so names are not split across lines.
  QStringList devsNbsp;
  for (const auto &dev : s_awesomeDevs) {
    QString withNbsp = dev;
    devsNbsp.append(withNbsp.replace(" ", QStringLiteral("&nbsp;")));
  }

  ui->lblImportantDevs->setTextFormat(Qt::RichText);
  ui->lblImportantDevs->setText(QStringLiteral("%1\n").arg(devsNbsp.join(", ")));

  ui->btnOk->setDefault(true);
  connect(ui->btnOk, &QPushButton::clicked, this, &AboutDialog::close);
}

void AboutDialog::copyVersionText() const
{
#ifdef DESKFLOW_NO_CLIPBOARD
  // never connected in this build; kept only so the header stays unchanged
#else
  QString infoString = QStringLiteral("%1: %2 (%3)\nQt: %4\nSystem: %5")
                           .arg(kAppName, kVersion, kVersionGitSha, qVersion(), QSysInfo::prettyProductName());
  if (Settings::isPortableMode()) {
    infoString.append(QStringLiteral("\nPortable Mode"));
  }
#ifdef Q_OS_LINUX
  infoString.append(QStringLiteral("\nSession: %1 (%2)")
                        .arg(qEnvironmentVariable("XDG_CURRENT_DESKTOP"), qEnvironmentVariable("XDG_SESSION_TYPE")));
#endif
  QGuiApplication::clipboard()->setText(infoString);
#endif
}

AboutDialog::~AboutDialog() = default;
