#pragma once

#include <QStringList>

// Commands that need no display, for scripts and agents: plate check, export,
// new, and skill. The window opens with plate open.
bool isCliCommand(const QString &word);
QString cliSummary();
int runCli(const QStringList &arguments);
