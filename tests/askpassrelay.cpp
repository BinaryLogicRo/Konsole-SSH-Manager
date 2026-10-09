#include "sshagent/askpass.h"

#include <QCoreApplication>

// Stands in for the app as ssh-add's askpass program, so tests never start the app.
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    return Askpass::run(app.arguments().value(1));
}
