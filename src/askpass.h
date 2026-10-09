#pragma once

class QString;

// The app doubles as ssh-add's askpass program, so the passphrase prompt never
// offers to remember the passphrase (as ksshaskpass does).
namespace Askpass
{
// True when ssh-add started the app to ask for a passphrase.
bool isRequested();
// Asks for the passphrase, writes it to stdout for ssh-add and returns the exit code.
int run(const QString &prompt);
}
