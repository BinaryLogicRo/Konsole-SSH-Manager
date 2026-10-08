#include "sshagentformat.h"

#include <QCryptographicHash>
#include <QList>
#include <QStringList>

namespace
{
const QLatin1String s_certificateSuffix("-cert-v01@openssh.com");

struct CertificateType {
    const char *certificate;
    const char *plain;
    int keyFields; // fields of the public key that follow the nonce
};

// Certificate types OpenSSH 9.2 and later know, with the plain key type inside them.
constexpr CertificateType s_certificateTypes[] = {
    {"ssh-rsa-cert-v01@openssh.com", "ssh-rsa", 2},
    {"ssh-dss-cert-v01@openssh.com", "ssh-dss", 4},
    {"ecdsa-sha2-nistp256-cert-v01@openssh.com", "ecdsa-sha2-nistp256", 2},
    {"ecdsa-sha2-nistp384-cert-v01@openssh.com", "ecdsa-sha2-nistp384", 2},
    {"ecdsa-sha2-nistp521-cert-v01@openssh.com", "ecdsa-sha2-nistp521", 2},
    {"ssh-ed25519-cert-v01@openssh.com", "ssh-ed25519", 1},
    {"sk-ecdsa-sha2-nistp256-cert-v01@openssh.com", "sk-ecdsa-sha2-nistp256@openssh.com", 3},
    {"sk-ssh-ed25519-cert-v01@openssh.com", "sk-ssh-ed25519@openssh.com", 2},
};

// Reads one SSH wire-format string (32-bit big-endian length, then the bytes).
bool readString(const QByteArray &data, qsizetype &pos, QByteArray *value)
{
    if (pos < 0 || data.size() - pos < 4) {
        return false;
    }
    const auto *bytes = reinterpret_cast<const uchar *>(data.constData() + pos);
    const quint32 length = (quint32(bytes[0]) << 24) | (quint32(bytes[1]) << 16) | (quint32(bytes[2]) << 8) | quint32(bytes[3]);
    if (length > quint32(data.size() - pos - 4)) {
        return false;
    }
    if (value) {
        *value = data.mid(pos + 4, qsizetype(length));
    }
    pos += 4 + qsizetype(length);
    return true;
}

void appendString(QByteArray &data, const QByteArray &value)
{
    const auto length = quint32(value.size());
    data.append(char(length >> 24)).append(char(length >> 16)).append(char(length >> 8)).append(char(length));
    data.append(value);
}

// The blob of the plain key inside a certificate; any other key's blob unchanged.
QByteArray plainKeyBlob(const QByteArray &blob)
{
    qsizetype pos = 0;
    QByteArray type;
    if (!readString(blob, pos, &type)) {
        return {};
    }
    for (const CertificateType &certificate : s_certificateTypes) {
        if (type != certificate.certificate) {
            continue;
        }
        if (!readString(blob, pos, nullptr)) { // nonce
            return {};
        }
        const qsizetype keyStart = pos;
        for (int i = 0; i < certificate.keyFields; ++i) {
            if (!readString(blob, pos, nullptr)) {
                return {};
            }
        }
        QByteArray plain;
        appendString(plain, certificate.plain);
        plain += blob.mid(keyStart, pos - keyStart);
        return plain;
    }
    return blob;
}
}

bool SshAgentKey::isCertificate() const
{
    return keyType.endsWith(s_certificateSuffix);
}

QList<SshAgentKey> SshAgentFormat::parsePublicKeys(const QByteArray &output)
{
    QList<SshAgentKey> keys;
    const QList<QByteArray> lines = output.split('\n');
    for (const QByteArray &rawLine : lines) {
        const QByteArray line = rawLine.trimmed();
        const qsizetype typeEnd = line.indexOf(' ');
        if (typeEnd <= 0) {
            continue;
        }
        qsizetype blobEnd = line.indexOf(' ', typeEnd + 1);
        if (blobEnd < 0) {
            blobEnd = line.size();
        }
        const QByteArray type = line.left(typeEnd);
        const auto decoded = QByteArray::fromBase64Encoding(line.mid(typeEnd + 1, blobEnd - typeEnd - 1), QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded) {
            continue;
        }
        // The blob starts with its own type, which must match the line's.
        qsizetype pos = 0;
        QByteArray blobType;
        if (!readString(*decoded, pos, &blobType) || blobType != type) {
            continue;
        }
        const QString fingerprint = SshAgentFormat::fingerprint(*decoded);
        if (fingerprint.isEmpty()) {
            continue;
        }
        SshAgentKey key;
        key.keyType = QString::fromLatin1(type);
        key.comment = QString::fromUtf8(line.mid(blobEnd).trimmed());
        key.fingerprint = fingerprint;
        key.publicKeyLine = line;
        keys.append(key);
    }
    return keys;
}

QString SshAgentFormat::fingerprint(const QByteArray &blob)
{
    const QByteArray plain = plainKeyBlob(blob);
    if (plain.isEmpty()) {
        return {};
    }
    const QByteArray hash = QCryptographicHash::hash(plain, QCryptographicHash::Sha256);
    return QStringLiteral("SHA256:") + QString::fromLatin1(hash.toBase64(QByteArray::OmitTrailingEquals));
}

QString SshAgentFormat::displayType(QStringView keyType)
{
    QString type = keyType.toString();
    const bool certificate = type.endsWith(s_certificateSuffix);
    if (certificate) {
        type.chop(s_certificateSuffix.size());
    } else if (type.endsWith(QLatin1String("@openssh.com"))) {
        type.chop(QLatin1String("@openssh.com").size());
    }

    QString name;
    if (type == QLatin1String("ssh-rsa")) {
        name = QStringLiteral("RSA");
    } else if (type == QLatin1String("ssh-dss")) {
        name = QStringLiteral("DSA");
    } else if (type == QLatin1String("ssh-ed25519")) {
        name = QStringLiteral("ED25519");
    } else if (type.startsWith(QLatin1String("ecdsa-sha2-"))) {
        name = QStringLiteral("ECDSA");
    } else if (type == QLatin1String("sk-ssh-ed25519")) {
        name = QStringLiteral("ED25519-SK");
    } else if (type == QLatin1String("sk-ecdsa-sha2-nistp256")) {
        name = QStringLiteral("ECDSA-SK");
    } else {
        return keyType.toString();
    }
    return certificate ? name + QStringLiteral("-CERT") : name;
}

std::optional<SshKeyFileInfo> SshAgentFormat::parseFingerprint(const QByteArray &output)
{
    // <bits> <fingerprint> <comment, may contain spaces> (<TYPE>)
    const qsizetype lineEnd = output.indexOf('\n');
    const QString line = QString::fromUtf8(lineEnd < 0 ? output : output.left(lineEnd)).trimmed();
    const qsizetype bitsEnd = line.indexOf(QLatin1Char(' '));
    const qsizetype fingerprintEnd = line.indexOf(QLatin1Char(' '), bitsEnd + 1);
    const qsizetype typeStart = line.lastIndexOf(QLatin1String(" ("));
    if (bitsEnd <= 0 || fingerprintEnd < 0 || typeStart < fingerprintEnd || !line.endsWith(QLatin1Char(')'))) {
        return std::nullopt;
    }
    bool ok = false;
    SshKeyFileInfo info;
    info.bits = line.left(bitsEnd).toInt(&ok);
    info.fingerprint = line.mid(bitsEnd + 1, fingerprintEnd - bitsEnd - 1);
    if (!ok || !info.fingerprint.startsWith(QLatin1String("SHA256:"))) {
        return std::nullopt;
    }
    info.comment = line.mid(fingerprintEnd + 1, typeStart - fingerprintEnd - 1).trimmed();
    if (info.comment == QLatin1String("no comment")) {
        info.comment.clear();
    }
    info.type = line.mid(typeStart + 2, line.size() - typeStart - 3);
    return info;
}

QString SshAgentFormat::message(const QByteArray &output)
{
    QStringList parts;
    const QStringList lines = QString::fromUtf8(output).split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        while (line.startsWith(QLatin1Char('@'))) {
            line.remove(0, 1);
        }
        while (line.endsWith(QLatin1Char('@'))) {
            line.chop(1);
        }
        line = line.trimmed();
        if (!line.isEmpty()) {
            parts.append(line);
        }
    }
    return parts.join(QLatin1Char(' '));
}
