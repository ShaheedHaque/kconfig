/*
 *
 *  This file is part of the KDE libraries
 *  Copyright (c) 2001 Waldo Bastian <bastian@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#include <config-kconf.h> // CMAKE_INSTALL_PREFIX

#include <QtCore/QDate>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QTextCodec>
#include <QUrl>
#include <QTemporaryFile>
#include <QCoreApplication>
#include <QtCore/QDir>
#include <QProcess>
#include <QDebug>

#include <kconfig.h>
#include <kconfiggroup.h>

#include <qstandardpaths.h>
#include <qcommandlineparser.h>
#include <qcommandlineoption.h>

#include "kconfigutils.h"

class KonfUpdate
{
public:
    KonfUpdate(QCommandLineParser *parser);
    ~KonfUpdate();
    QStringList findUpdateFiles(bool dirtyOnly);

    QTextStream &log();
    QTextStream &logFileError();

    bool checkFile(const QString &filename);
    void checkGotFile(const QString &_file, const QString &id);

    bool updateFile(const QString &filename);

    void gotId(const QString &_id);
    void gotFile(const QString &_file);
    void gotGroup(const QString &_group);
    void gotRemoveGroup(const QString &_group);
    void gotKey(const QString &_key);
    void gotRemoveKey(const QString &_key);
    void gotAllKeys();
    void gotAllGroups();
    void gotOptions(const QString &_options);
    void gotScript(const QString &_script);
    void gotScriptArguments(const QString &_arguments);
    void resetOptions();

    void copyGroup(const KConfigBase *cfg1, const QString &group1,
                   KConfigBase *cfg2, const QString &group2);
    void copyGroup(const KConfigGroup &cg1, KConfigGroup &cg2);
    void copyOrMoveKey(const QStringList &srcGroupPath, const QString &srcKey, const QStringList &dstGroupPath, const QString &dstKey);
    void copyOrMoveGroup(const QStringList &srcGroupPath, const QStringList &dstGroupPath);

    QStringList parseGroupString(const QString &_str);

protected:
    KConfig *m_config;
    QString m_currentFilename;
    bool m_skip;
    bool m_skipFile;
    bool m_debug;
    QString m_id;

    QString m_oldFile;
    QString m_newFile;
    QString m_newFileName;
    KConfig *m_oldConfig1; // Config to read keys from.
    KConfig *m_oldConfig2; // Config to delete keys from.
    KConfig *m_newConfig;

    QStringList m_oldGroup;
    QStringList m_newGroup;

    bool m_bCopy;
    bool m_bOverwrite;
    bool m_bUseConfigInfo;
    QString m_arguments;
    QTextStream *m_textStream;
    QFile *m_file;
    QString m_line;
    int m_lineCount;
};

KonfUpdate::KonfUpdate(QCommandLineParser *parser)
    : m_oldConfig1(Q_NULLPTR), m_oldConfig2(Q_NULLPTR), m_newConfig(Q_NULLPTR),
      m_bCopy(false), m_bOverwrite(false), m_textStream(Q_NULLPTR),
      m_file(Q_NULLPTR), m_lineCount(-1)
{
    bool updateAll = false;

    m_config = new KConfig(QStringLiteral("kconf_updaterc"));
    KConfigGroup cg(m_config, QString());

    QStringList updateFiles;

    m_debug = parser->isSet(QStringLiteral("debug"));

    if (parser->isSet(QStringLiteral("testmode"))) {
        QStandardPaths::setTestModeEnabled(true);
    }

    m_bUseConfigInfo = false;
    if (parser->isSet(QStringLiteral("check"))) {
        m_bUseConfigInfo = true;
        const QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kconf_update/" + parser->value(QStringLiteral("check")));
        if (file.isEmpty()) {
            qWarning("File '%s' not found.", parser->value(QStringLiteral("check")).toLocal8Bit().data());
            log() << "File '" << parser->value(QStringLiteral("check")) << "' passed on command line not found" << endl;
            return;
        }
        updateFiles.append(file);
    } else if (!parser->positionalArguments().isEmpty()) {
        updateFiles += parser->positionalArguments();
    } else {
        if (cg.readEntry("autoUpdateDisabled", false)) {
            return;
        }
        updateFiles = findUpdateFiles(true);
        updateAll = true;
    }

    foreach (const QString& file, updateFiles) {
        updateFile(file);
    }

    if (updateAll && !cg.readEntry("updateInfoAdded", false)) {
        cg.writeEntry("updateInfoAdded", true);
        updateFiles = findUpdateFiles(false);

        for (QStringList::ConstIterator it = updateFiles.constBegin();
                it != updateFiles.constEnd();
                ++it) {
            checkFile(*it);
        }
        updateFiles.clear();
    }
}

KonfUpdate::~KonfUpdate()
{
    delete m_config;
    delete m_file;
    delete m_textStream;
}

static QTextStream &operator<<(QTextStream &stream, const QStringList &lst)
{
    stream << lst.join(QStringLiteral(", "));
    return stream;
}

QTextStream &
KonfUpdate::log()
{
    if (!m_textStream) {
#if 0
        QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Char('/') + "kconf_update/log";
        QDir().mkpath(dir);
        QString file = dir + "/update.log";
        m_file = new QFile(file);
        if (m_file->open(QIODevice::WriteOnly | QIODevice::Append)) {
            m_textStream = new QTextStream(m_file);
        } else {
            // Error
            m_textStream = new QTextStream(stderr, QIODevice::WriteOnly);
        }
#endif
        m_textStream = new QTextStream(stderr, QIODevice::WriteOnly);
    }

    (*m_textStream) << QDateTime::currentDateTime().toString(Qt::ISODate) << " ";

    return *m_textStream;
}

QTextStream &
KonfUpdate::logFileError()
{
    return log() << m_currentFilename << ':' << m_lineCount << ":'" << m_line << "': ";
}

QStringList KonfUpdate::findUpdateFiles(bool dirtyOnly)
{
    QStringList result;

    const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("kconf_update"), QStandardPaths::LocateDirectory);
    Q_FOREACH (const QString &d, dirs) {
        const QDir dir(d);

        const QStringList fileNames = dir.entryList(QStringList(QStringLiteral("*.upd")));
        Q_FOREACH (const QString &fileName, fileNames) {
            const QString file = dir.filePath(fileName);
            QFileInfo info(file);

            KConfigGroup cg(m_config, fileName);
            const QDateTime ctime = QDateTime::fromTime_t(cg.readEntry("ctime", 0u));
            const QDateTime mtime = QDateTime::fromTime_t(cg.readEntry("mtime", 0u));
            if (!dirtyOnly ||
                    (ctime != info.created()) || (mtime != info.lastModified())) {
                result.append(file);
            }
        }
    }
    return result;
}

bool KonfUpdate::checkFile(const QString &filename)
{
    m_currentFilename = filename;
    int i = m_currentFilename.lastIndexOf('/');
    if (i != -1) {
        m_currentFilename = m_currentFilename.mid(i + 1);
    }
    m_skip = true;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QTextStream ts(&file);
    ts.setCodec(QTextCodec::codecForName("ISO-8859-1"));
    int lineCount = 0;
    resetOptions();
    QString id;
    bool foundVersion = false;
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line.startsWith(QLatin1String("Version=5"))) {
            foundVersion = true;
        }
        ++lineCount;
        if (line.isEmpty() || (line[0] == '#')) {
            continue;
        }
        if (line.startsWith(QLatin1String("Id="))) {
            if (!foundVersion) {
                qDebug() << QStringLiteral("Missing \"Version=5\", file \'%1\' will be skipped.").arg(filename);
                return true;
            }
            id = m_currentFilename + ':' + line.mid(3);
        } else if (line.startsWith(QLatin1String("File="))) {
            checkGotFile(line.mid(5), id);
        }
    }

    return true;
}

void KonfUpdate::checkGotFile(const QString &_file, const QString &id)
{
    QString file;
    int i = _file.indexOf(',');
    if (i == -1) {
        file = _file.trimmed();
    } else {
        file = _file.mid(i + 1).trimmed();
    }

//   qDebug("File %s, id %s", file.toLatin1().constData(), id.toLatin1().constData());

    KConfig cfg(file, KConfig::SimpleConfig);
    KConfigGroup cg = cfg.group("$Version");
    QStringList ids = cg.readEntry("update_info", QStringList());
    if (ids.contains(id)) {
        return;
    }
    ids.append(id);
    cg.writeEntry("update_info", ids);
}

/**
 * Syntax:
 * # Comment
 * Id=id
 * File=oldfile[,newfile]
 * AllGroups
 * Group=oldgroup[,newgroup]
 * RemoveGroup=oldgroup
 * Options=[copy,][overwrite,]
 * Key=oldkey[,newkey]
 * RemoveKey=ldkey
 * AllKeys
 * Keys= [Options](AllKeys|(Key|RemoveKey)*)
 * ScriptArguments=arguments
 * Script=scriptfile[,interpreter]
 *
 * Sequence:
 * (Id,(File(Group,Keys)*)*)*
 **/
bool KonfUpdate::updateFile(const QString &filename)
{
    m_currentFilename = filename;
    int i = m_currentFilename.lastIndexOf('/');
    if (i != -1) {
        m_currentFilename = m_currentFilename.mid(i + 1);
    }
    m_skip = true;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    log() << "Checking update-file '" << filename << "' for new updates" << endl;

    QTextStream ts(&file);
    ts.setCodec(QTextCodec::codecForName("ISO-8859-1"));
    m_lineCount = 0;
    resetOptions();
    bool foundVersion = false;
    while (!ts.atEnd()) {
        m_line = ts.readLine().trimmed();
        if (m_line.startsWith(QLatin1String("Version=5"))) {
            foundVersion = true;
        }
        m_lineCount++;
        if (m_line.isEmpty() || (m_line[0] == QLatin1Char('#'))) {
            continue;
        }
        if (m_line.startsWith(QLatin1String("Id="))) {
            if (!foundVersion) {
                qDebug() << QStringLiteral("Missing \"Version=5\", file \'%1\' will be skipped.").arg(filename);
                break;
            }
            gotId(m_line.mid(3));
        } else if (m_skip) {
            continue;
        } else if (m_line.startsWith(QLatin1String("Options="))) {
            gotOptions(m_line.mid(8));
        } else if (m_line.startsWith(QLatin1String("File="))) {
            gotFile(m_line.mid(5));
        } else if (m_skipFile) {
            continue;
        } else if (m_line.startsWith(QLatin1String("Group="))) {
            gotGroup(m_line.mid(6));
        } else if (m_line.startsWith(QLatin1String("RemoveGroup="))) {
            gotRemoveGroup(m_line.mid(12));
            resetOptions();
        } else if (m_line.startsWith(QLatin1String("Script="))) {
            gotScript(m_line.mid(7));
            resetOptions();
        } else if (m_line.startsWith(QLatin1String("ScriptArguments="))) {
            gotScriptArguments(m_line.mid(16));
        } else if (m_line.startsWith(QLatin1String("Key="))) {
            gotKey(m_line.mid(4));
            resetOptions();
        } else if (m_line.startsWith(QLatin1String("RemoveKey="))) {
            gotRemoveKey(m_line.mid(10));
            resetOptions();
        } else if (m_line == QLatin1String("AllKeys")) {
            gotAllKeys();
            resetOptions();
        } else if (m_line == QLatin1String("AllGroups")) {
            gotAllGroups();
            resetOptions();
        } else {
            logFileError() << "Parse error" << endl;
        }
    }
    // Flush.
    gotId(QString());

    QFileInfo info(filename);
    KConfigGroup cg(m_config, m_currentFilename);
    cg.writeEntry("ctime", info.created().toTime_t());
    cg.writeEntry("mtime", info.lastModified().toTime_t());
    cg.sync();
    return true;
}

void KonfUpdate::gotId(const QString &_id)
{
    if (!m_id.isEmpty() && !m_skip) {
        KConfigGroup cg(m_config, m_currentFilename);

        QStringList ids = cg.readEntry("done", QStringList());
        if (!ids.contains(m_id)) {
            ids.append(m_id);
            cg.writeEntry("done", ids);
            cg.sync();
        }
    }

    // Flush pending changes
    gotFile(QString());
    KConfigGroup cg(m_config, m_currentFilename);

    QStringList ids = cg.readEntry("done", QStringList());
    if (!_id.isEmpty()) {
        if (ids.contains(_id)) {
            //qDebug("Id '%s' was already in done-list", _id.toLatin1().constData());
            if (!m_bUseConfigInfo) {
                m_skip = true;
                return;
            }
        }
        m_skip = false;
        m_skipFile = false;
        m_id = _id;
        if (m_bUseConfigInfo) {
            log() << m_currentFilename << ": Checking update '" << _id << "'" << endl;
        } else {
            log() << m_currentFilename << ": Found new update '" << _id << "'" << endl;
        }
    }
}

void KonfUpdate::gotFile(const QString &_file)
{
    // Reset group
    gotGroup(QString());

    if (!m_oldFile.isEmpty()) {
        // Close old file.
        delete m_oldConfig1;
        m_oldConfig1 = Q_NULLPTR;

        KConfigGroup cg(m_oldConfig2, "$Version");
        QStringList ids = cg.readEntry("update_info", QStringList());
        QString cfg_id = m_currentFilename + ':' + m_id;
        if (!ids.contains(cfg_id) && !m_skip) {
            ids.append(cfg_id);
            cg.writeEntry("update_info", ids);
        }
        cg.sync();
        delete m_oldConfig2;
        m_oldConfig2 = Q_NULLPTR;

        QString file = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1Char('/') + m_oldFile;
        QFileInfo info(file);
        if (info.exists() && info.size() == 0) {
            // Delete empty file.
            QFile::remove(file);
        }

        m_oldFile.clear();
    }
    if (!m_newFile.isEmpty()) {
        // Close new file.
        KConfigGroup cg(m_newConfig, "$Version");
        QStringList ids = cg.readEntry("update_info", QStringList());
        QString cfg_id = m_currentFilename + ':' + m_id;
        if (!ids.contains(cfg_id) && !m_skip) {
            ids.append(cfg_id);
            cg.writeEntry("update_info", ids);
        }
        m_newConfig->sync();
        delete m_newConfig;
        m_newConfig = Q_NULLPTR;

        m_newFile.clear();
    }
    m_newConfig = Q_NULLPTR;

    int i = _file.indexOf(',');
    if (i == -1) {
        m_oldFile = _file.trimmed();
    } else {
        m_oldFile = _file.left(i).trimmed();
        m_newFile = _file.mid(i + 1).trimmed();
        if (m_oldFile == m_newFile) {
            m_newFile.clear();
        }
    }

    if (!m_oldFile.isEmpty()) {
        m_oldConfig2 = new KConfig(m_oldFile, KConfig::NoGlobals);
        QString cfg_id = m_currentFilename + ':' + m_id;
        KConfigGroup cg(m_oldConfig2, "$Version");
        QStringList ids = cg.readEntry("update_info", QStringList());
        if (ids.contains(cfg_id)) {
            m_skip = true;
            m_newFile.clear();
            log() << m_currentFilename << ": Skipping update '" << m_id << "'" << endl;
        }

        if (!m_newFile.isEmpty()) {
            m_newConfig = new KConfig(m_newFile, KConfig::NoGlobals);
            KConfigGroup cg(m_newConfig, "$Version");
            ids = cg.readEntry("update_info", QStringList());
            if (ids.contains(cfg_id)) {
                m_skip = true;
                log() << m_currentFilename << ": Skipping update '" << m_id << "'" << endl;
            }
        } else {
            m_newConfig = m_oldConfig2;
        }

        m_oldConfig1 = new KConfig(m_oldFile, KConfig::NoGlobals);
    } else {
        m_newFile.clear();
    }
    m_newFileName = m_newFile;
    if (m_newFileName.isEmpty()) {
        m_newFileName = m_oldFile;
    }

    m_skipFile = false;
    if (!m_oldFile.isEmpty()) { // if File= is specified, it doesn't exist, is empty or contains only kconf_update's [$Version] group, skip
        if (m_oldConfig1 != Q_NULLPTR
                && (m_oldConfig1->groupList().isEmpty()
                    || (m_oldConfig1->groupList().count() == 1 && m_oldConfig1->groupList().at(0) == QLatin1String("$Version")))) {
            log() << m_currentFilename << ": File '" << m_oldFile << "' does not exist or empty, skipping" << endl;
            m_skipFile = true;
        }
    }
}

QStringList KonfUpdate::parseGroupString(const QString &str)
{
    bool ok;
    QString error;
    QStringList lst = KConfigUtils::parseGroupString(str, &ok, &error);
    if (!ok) {
        logFileError() << error;
    }
    return lst;
}

void KonfUpdate::gotGroup(const QString &_group)
{
    QString group = _group.trimmed();
    if (group.isEmpty()) {
        m_oldGroup = m_newGroup = QStringList();
        return;
    }

    QStringList tokens = group.split(',');
    m_oldGroup = parseGroupString(tokens.at(0));
    if (tokens.count() == 1) {
        m_newGroup = m_oldGroup;
    } else {
        m_newGroup = parseGroupString(tokens.at(1));
    }
}

void KonfUpdate::gotRemoveGroup(const QString &_group)
{
    m_oldGroup = parseGroupString(_group);

    if (!m_oldConfig1) {
        logFileError() << "RemoveGroup without previous File specification" << endl;
        return;
    }

    KConfigGroup cg = KConfigUtils::openGroup(m_oldConfig2, m_oldGroup);
    if (!cg.exists()) {
        return;
    }
    // Delete group.
    cg.deleteGroup();
    log() << m_currentFilename << ": RemoveGroup removes group " << m_oldFile << ":" << m_oldGroup << endl;
}

void KonfUpdate::gotKey(const QString &_key)
{
    QString oldKey, newKey;
    int i = _key.indexOf(',');
    if (i == -1) {
        oldKey = _key.trimmed();
        newKey = oldKey;
    } else {
        oldKey = _key.left(i).trimmed();
        newKey = _key.mid(i + 1).trimmed();
    }

    if (oldKey.isEmpty() || newKey.isEmpty()) {
        logFileError() << "Key specifies invalid key" << endl;
        return;
    }
    if (!m_oldConfig1) {
        logFileError() << "Key without previous File specification" << endl;
        return;
    }
    copyOrMoveKey(m_oldGroup, oldKey, m_newGroup, newKey);
}

void KonfUpdate::copyOrMoveKey(const QStringList &srcGroupPath, const QString &srcKey, const QStringList &dstGroupPath, const QString &dstKey)
{
    KConfigGroup dstCg = KConfigUtils::openGroup(m_newConfig, dstGroupPath);
    if (!m_bOverwrite && dstCg.hasKey(dstKey)) {
        log() << m_currentFilename << ": Skipping " << m_newFileName << ":" << dstCg.name() << ":" << dstKey << ", already exists." << endl;
        return;
    }

    KConfigGroup srcCg = KConfigUtils::openGroup(m_oldConfig1, srcGroupPath);
    if (!srcCg.hasKey(srcKey)) {
        return;
    }
    QString value = srcCg.readEntry(srcKey, QString());
    log() << m_currentFilename << ": Updating " << m_newFileName << ":" << dstCg.name() << ":" << dstKey << " to '" << value << "'" << endl;
    dstCg.writeEntry(dstKey, value);

    if (m_bCopy) {
        return; // Done.
    }

    // Delete old entry
    if (m_oldConfig2 == m_newConfig
            && srcGroupPath == dstGroupPath
            && srcKey == dstKey) {
        return; // Don't delete!
    }
    KConfigGroup srcCg2 = KConfigUtils::openGroup(m_oldConfig2, srcGroupPath);
    srcCg2.deleteEntry(srcKey);
    log() << m_currentFilename << ": Removing " << m_oldFile << ":" << srcCg2.name() << ":" << srcKey << ", moved." << endl;
}

void KonfUpdate::copyOrMoveGroup(const QStringList &srcGroupPath, const QStringList &dstGroupPath)
{
    KConfigGroup cg = KConfigUtils::openGroup(m_oldConfig1, srcGroupPath);

    // Keys
    Q_FOREACH (const QString &key, cg.keyList()) {
        copyOrMoveKey(srcGroupPath, key, dstGroupPath, key);
    }

    // Subgroups
    Q_FOREACH (const QString &group, cg.groupList()) {
        const QStringList groupPath(group);
        copyOrMoveGroup(srcGroupPath + groupPath, dstGroupPath + groupPath);
    }
}

void KonfUpdate::gotRemoveKey(const QString &_key)
{
    QString key = _key.trimmed();

    if (key.isEmpty()) {
        logFileError() << "RemoveKey specifies invalid key" << endl;
        return;
    }

    if (!m_oldConfig1) {
        logFileError() << "Key without previous File specification" << endl;
        return;
    }

    KConfigGroup cg1 = KConfigUtils::openGroup(m_oldConfig1, m_oldGroup);
    if (!cg1.hasKey(key)) {
        return;
    }
    log() << m_currentFilename << ": RemoveKey removes " << m_oldFile << ":" << m_oldGroup << ":" << key << endl;

    // Delete old entry
    KConfigGroup cg2 = KConfigUtils::openGroup(m_oldConfig2, m_oldGroup);
    cg2.deleteEntry(key);
    /*if (m_oldConfig2->deleteGroup(m_oldGroup, KConfig::Normal)) { // Delete group if empty.
       log() << m_currentFilename << ": Removing empty group " << m_oldFile << ":" << m_oldGroup << endl;
    }   (this should be automatic)*/
}

void KonfUpdate::gotAllKeys()
{
    if (!m_oldConfig1) {
        logFileError() << "AllKeys without previous File specification" << endl;
        return;
    }

    copyOrMoveGroup(m_oldGroup, m_newGroup);
}

void KonfUpdate::gotAllGroups()
{
    if (!m_oldConfig1) {
        logFileError() << "AllGroups without previous File specification" << endl;
        return;
    }

    const QStringList allGroups = m_oldConfig1->groupList();
    for (QStringList::ConstIterator it = allGroups.begin();
            it != allGroups.end(); ++it) {
        m_oldGroup = QStringList() << *it;
        m_newGroup = m_oldGroup;
        gotAllKeys();
    }
}

void KonfUpdate::gotOptions(const QString &_options)
{
    const QStringList options = _options.split(',');
    for (QStringList::ConstIterator it = options.begin();
            it != options.end();
            ++it) {
        if ((*it).toLower().trimmed() == QLatin1String("copy")) {
            m_bCopy = true;
        }

        if ((*it).toLower().trimmed() == QLatin1String("overwrite")) {
            m_bOverwrite = true;
        }
    }
}

void KonfUpdate::copyGroup(const KConfigBase *cfg1, const QString &group1,
                           KConfigBase *cfg2, const QString &group2)
{
    KConfigGroup cg2 = cfg2->group(group2);
    copyGroup(cfg1->group(group1), cg2);
}

void KonfUpdate::copyGroup(const KConfigGroup &cg1, KConfigGroup &cg2)
{
    // Copy keys
    QMap<QString, QString> list = cg1.entryMap();
    for (QMap<QString, QString>::ConstIterator it = list.constBegin();
            it != list.constEnd(); ++it) {
        if (m_bOverwrite || !cg2.hasKey(it.key())) {
            cg2.writeEntry(it.key(), it.value());
        }
    }

    // Copy subgroups
    Q_FOREACH (const QString &group, cg1.groupList()) {
        copyGroup(&cg1, group, &cg2, group);
    }
}

void KonfUpdate::gotScriptArguments(const QString &_arguments)
{
    m_arguments = _arguments;
}

void KonfUpdate::gotScript(const QString &_script)
{
    QString script, interpreter;
    int i = _script.indexOf(',');
    if (i == -1) {
        script = _script.trimmed();
    } else {
        script = _script.left(i).trimmed();
        interpreter = _script.mid(i + 1).trimmed();
    }

    if (script.isEmpty()) {
        logFileError() << "Script fails to specify filename";
        m_skip = true;
        return;
    }

    QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kconf_update/") + script);
    if (path.isEmpty()) {
        if (interpreter.isEmpty()) {
            path = CMAKE_INSTALL_PREFIX "/" LIB_INSTALL_DIR "/kconf_update_bin/" + script;
            if (!QFile::exists(path)) {
                path = QStandardPaths::findExecutable(script);
            }
        }

        if (path.isEmpty()) {
            logFileError() << "Script '" << script << "' not found" << endl;
            m_skip = true;
            return;
        }
    }

    if (!m_arguments.isNull()) {
        log() << m_currentFilename << ": Running script '" << script << "' with arguments '" << m_arguments << "'" << endl;
    } else {
        log() << m_currentFilename << ": Running script '" << script << "'" << endl;
    }

    QStringList args;
    QString cmd;
    if (interpreter.isEmpty()) {
        cmd = path;
    } else {
        QString interpreterPath = QStandardPaths::findExecutable(interpreter);
        if (interpreterPath.isEmpty()) {
            logFileError() << "Cannot find interpreter '" << interpreter << "'." << endl;
            m_skip = true;
            return;
        }
        cmd = interpreterPath;
        args << path;
    }

    if (!m_arguments.isNull()) {
        args += m_arguments;
    }

    QTemporaryFile scriptIn;
    scriptIn.open();
    QTemporaryFile scriptOut;
    scriptOut.open();

    int result;
    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.setStandardInputFile(scriptIn.fileName());
    proc.setStandardOutputFile(scriptOut.fileName());
    if (m_oldConfig1) {
        if (m_debug) {
            scriptIn.setAutoRemove(false);
            log() << "Script input stored in " << scriptIn.fileName() << endl;
        }
        KConfig cfg(scriptIn.fileName(), KConfig::SimpleConfig);

        if (m_oldGroup.isEmpty()) {
            // Write all entries to tmpFile;
            const QStringList grpList = m_oldConfig1->groupList();
            for (QStringList::ConstIterator it = grpList.begin();
                    it != grpList.end();
                    ++it) {
                copyGroup(m_oldConfig1, *it, &cfg, *it);
            }
        } else {
            KConfigGroup cg1 = KConfigUtils::openGroup(m_oldConfig1, m_oldGroup);
            KConfigGroup cg2(&cfg, QString());
            copyGroup(cg1, cg2);
        }
        cfg.sync();
    }
    if (m_debug) {
        log() << "About to run " << cmd << endl;
        QFile scriptFile(path);
        if (scriptFile.open(QIODevice::ReadOnly)) {
            log() << "Script contents is:" << endl << scriptFile.readAll() << endl;
        }
    }
    proc.start(cmd, args);
    if (!proc.waitForFinished(60000)) {
        logFileError() << "update script did not terminate within 60 seconds: " << cmd << endl;
        m_skip = true;
        return;
    }
    result = proc.exitCode();

    // Copy script stderr to log file
    {
        QTextStream ts(proc.readAllStandardError());
        ts.setCodec(QTextCodec::codecForName("UTF-8"));
        while (!ts.atEnd()) {
            QString line = ts.readLine();
            log() << "[Script] " << line << endl;
        }
    }
    proc.close();

    if (result) {
        log() << m_currentFilename << ": !! An error occurred while running '" << cmd << "'" << endl;
        return;
    }

    if (m_debug) {
        log() << "Successfully ran " << cmd << endl;
    }

    if (!m_oldConfig1) {
        return; // Nothing to merge
    }

    if (m_debug) {
        scriptOut.setAutoRemove(false);
        log() << "Script output stored in " << scriptOut.fileName() << endl;
        QFile output(scriptOut.fileName());
        if (output.open(QIODevice::ReadOnly)) {
            log() << "Script output is:" << endl << output.readAll() << endl;
        }
    }

    // Deleting old entries
    {
        QStringList group = m_oldGroup;
        QFile output(scriptOut.fileName());
        if (output.open(QIODevice::ReadOnly)) {
            QTextStream ts(&output);
            ts.setCodec(QTextCodec::codecForName("UTF-8"));
            while (!ts.atEnd()) {
                QString line = ts.readLine();
                if (line.startsWith('[')) {
                    group = parseGroupString(line);
                } else if (line.startsWith(QLatin1String("# DELETE "))) {
                    QString key = line.mid(9);
                    if (key[0] == '[') {
                        int j = key.lastIndexOf(']') + 1;
                        if (j > 0) {
                            group = parseGroupString(key.left(j));
                            key = key.mid(j);
                        }
                    }
                    KConfigGroup cg = KConfigUtils::openGroup(m_oldConfig2, group);
                    cg.deleteEntry(key);
                    log() << m_currentFilename << ": Script removes " << m_oldFile << ":" << group << ":" << key << endl;
                    /*if (m_oldConfig2->deleteGroup(group, KConfig::Normal)) { // Delete group if empty.
                       log() << m_currentFilename << ": Removing empty group " << m_oldFile << ":" << group << endl;
                    } (this should be automatic)*/
                } else if (line.startsWith(QLatin1String("# DELETEGROUP"))) {
                    QString str = line.mid(13).trimmed();
                    if (!str.isEmpty()) {
                        group = parseGroupString(str);
                    }
                    KConfigGroup cg = KConfigUtils::openGroup(m_oldConfig2, group);
                    cg.deleteGroup();
                    log() << m_currentFilename << ": Script removes group " << m_oldFile << ":" << group << endl;
                }
            }
        }
    }

    // Merging in new entries.
    KConfig scriptOutConfig(scriptOut.fileName(), KConfig::NoGlobals);
    if (m_newGroup.isEmpty()) {
        // Copy "default" keys as members of "default" keys
        copyGroup(&scriptOutConfig, QString(), m_newConfig, QString());
    } else {
        // Copy default keys as members of m_newGroup
        KConfigGroup srcCg = KConfigUtils::openGroup(&scriptOutConfig, QStringList());
        KConfigGroup dstCg = KConfigUtils::openGroup(m_newConfig, m_newGroup);
        copyGroup(srcCg, dstCg);
    }
    Q_FOREACH (const QString &group, scriptOutConfig.groupList()) {
        copyGroup(&scriptOutConfig, group, m_newConfig, group);
    }
}

void KonfUpdate::resetOptions()
{
    m_bCopy = false;
    m_bOverwrite = false;
    m_arguments.clear();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationVersion(QStringLiteral("1.1"));

    QCommandLineParser parser;
    parser.addVersionOption();
    parser.setApplicationDescription(QCoreApplication::translate("main", "KDE Tool for updating user configuration files"));
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("debug"), QCoreApplication::translate("main", "Keep output results from scripts")));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("testmode"), QCoreApplication::translate("main", "For unit tests only: use test directories to stay away from the user's real files")));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("check"), QCoreApplication::translate("main", "Check whether config file itself requires updating"), QStringLiteral("update-file")));
    parser.addPositionalArgument(QStringLiteral("files"), QCoreApplication::translate("main", "File(s) to read update instructions from"), QStringLiteral("[files...]"));

    // TODO aboutData.addAuthor(ki18n("Waldo Bastian"), KLocalizedString(), "bastian@kde.org");

    parser.process(app);
    KonfUpdate konfUpdate(&parser);

    return 0;
}
