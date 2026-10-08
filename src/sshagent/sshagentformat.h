#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringView>

#include <optional>

// One key held by the SSH agent, as listed by `ssh-add -L`.
struct SshAgentKey {
    QString keyType; // e.g. ssh-ed25519, or ssh-ed25519-cert-v01@openssh.com
    QString comment;
    // SHA256 fingerprint, as `ssh-keygen -l` prints it. For a certificate this
    // is the fingerprint of the key inside it, so it matches the key's file.
    QString fingerprint;
    QByteArray publicKeyLine; // the line as ssh-add printed it, used to remove the key

    bool isCertificate() const;
};

// What `ssh-keygen -l -f <file>` reports for a key file.
struct SshKeyFileInfo {
    int bits = 0;
    QString fingerprint;
    QString comment;
    QString type; // as ssh-keygen prints it, e.g. ED25519

    bool operator==(const SshKeyFileInfo &other) const = default;
};

// Reads the output of ssh-add and ssh-keygen. Never sees private key contents.
namespace SshAgentFormat
{
// Keys from `ssh-add -L` output. Lines that aren't valid public keys are skipped.
QList<SshAgentKey> parsePublicKeys(const QByteArray &output);
// SHA256 fingerprint of a public key blob (of the inner key for a certificate),
// or empty if the blob is malformed.
QString fingerprint(const QByteArray &blob);
// Short key type as ssh-keygen prints it (ED25519, RSA, ECDSA-SK, ED25519-CERT, …).
QString displayType(QStringView keyType);
// The first line of `ssh-keygen -l -E sha256 -f <file>` output.
std::optional<SshKeyFileInfo> parseFingerprint(const QByteArray &output);
// ssh-add's or ssh-keygen's message as one line, without the '@' frames of its warnings.
QString message(const QByteArray &output);
}
