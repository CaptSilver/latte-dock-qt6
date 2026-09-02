/*
    SPDX-FileCopyrightText: 2019 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "factory.h"

// local
#include "../layouts/importer.h"

// Qt
#include <QDebug>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTimer>
#include <QLatin1String>

// KDE
#include <KDirWatch>
#include <KLocalizedString>
#include <KMessageBox>
#include <KNotification>
#include <KPluginMetaData>
#include <KArchive/KTar>
#include <KArchive/KZip>
#include <KArchive/KArchiveEntry>
#include <KArchive/KArchiveDirectory>
#include <KNSWidgets/Dialog>

// C++
#include <memory>

namespace Latte {
namespace Indicator {

namespace {

//! The id becomes a directory name under the indicators tree and the argument
//! kpackagetool6 removes, so it has to stay a single harmless path component.
bool pluginIdIsSafe(const QString &id)
{
    static const QRegularExpression validId(QRegularExpression::anchoredPattern(QStringLiteral("[A-Za-z0-9._-]+")));

    return !id.isEmpty() && id != QLatin1String(".") && id != QLatin1String("..") && validId.match(id).hasMatch();
}

}

Factory::Factory(QObject *parent)
    : QObject(parent)
{
    m_parentWidget = new QWidget();

    m_mainPaths = Latte::Layouts::Importer::standardPaths();

    for(int i=0; i<m_mainPaths.count(); ++i) {
        m_mainPaths[i] = m_mainPaths[i] + QStringLiteral("/latte/indicators");
        discoverNewIndicators(m_mainPaths[i]);
    }

    //! track paths for changes
    for(const auto &dir : m_mainPaths) {
        KDirWatch::self()->addDir(dir);
    }

    connect(KDirWatch::self(), &KDirWatch::dirty, this, [ & ](const QString & path) {
        if (m_indicatorsPaths.contains(path)) {
            //! indicator updated
            reload(path);
        } else if (m_mainPaths.contains(path)){
            //! consider indicator addition
            discoverNewIndicators(path);
        }
    });

    connect(KDirWatch::self(), &KDirWatch::deleted, this, [ & ](const QString & path) {
        if (m_indicatorsPaths.contains(path)) {
            //! indicator removed
            removeIndicatorRecords(path);
        }
    });

    qDebug() << m_plugins[QStringLiteral("org.kde.latte.default")].name();
}

Factory::~Factory()
{
    m_parentWidget->deleteLater();
}

bool Factory::pluginExists(QString id) const
{
    return m_plugins.contains(id);
}

int Factory::customPluginsCount()
{
    return m_customPluginIds.count();
}

QStringList Factory::customPluginIds()
{
    return m_customPluginIds;
}

QStringList Factory::customPluginNames()
{
    return m_customPluginNames;
}

QStringList Factory::customLocalPluginIds()
{
    return m_customLocalPluginIds;
}

KPluginMetaData Factory::metadata(QString pluginId)
{
    if (m_plugins.contains(pluginId)) {
        return m_plugins[pluginId];
    }

    return KPluginMetaData();
}

void Factory::reload(const QString &indicatorPath)
{
    QString pluginChangedId;

    if (!indicatorPath.isEmpty() && indicatorPath != QLatin1String(".") && indicatorPath != QLatin1String("..")) {
        QString metadataFile = metadataFileAbsolutePath(indicatorPath);

        if(QFileInfo(metadataFile).exists()) {
            KPluginMetaData metadata = KPluginMetaData::fromJsonFile(metadataFile);

            if (metadataAreValid(metadata)) {
                pluginChangedId = metadata.pluginId();
                QString uiFile = indicatorPath + QStringLiteral("/package/") + metadata.value(QStringLiteral("X-Latte-MainScript"));

                if (!m_plugins.contains(metadata.pluginId())) {
                    m_plugins[metadata.pluginId()] = metadata;
                }

                if (QFileInfo(uiFile).exists()) {
                    m_pluginUiPaths[metadata.pluginId()] = QFileInfo(uiFile).absolutePath();
                }

                if ((metadata.pluginId() != QLatin1String("org.kde.latte.default"))
                        && (metadata.pluginId() != QLatin1String("org.kde.latte.plasma"))
                        && (metadata.pluginId() != QLatin1String("org.kde.latte.plasmatabstyle"))) {

                    //! m_customPluginIds and m_customPluginNames are paired by index,
                    //! so a new id and its name must be inserted together under the
                    //! same id-uniqueness guard; gating the name on its own
                    //! !contains(name) check desyncs the two lists when two custom
                    //! indicators share a display name but have distinct ids.
                    if (!m_customPluginIds.contains(metadata.pluginId())) {
                        //! find correct alphabetical position
                        int newPos = -1;

                        for (int i=0; i<m_customPluginNames.count(); ++i) {
                            if (QString::compare(metadata.name(), m_customPluginNames[i], Qt::CaseInsensitive)<=0) {
                                newPos = i;
                                break;
                            }
                        }

                        if (newPos == -1) {
                            m_customPluginIds << metadata.pluginId();
                            m_customPluginNames << metadata.name();
                        } else {
                            m_customPluginIds.insert(newPos, metadata.pluginId());
                            m_customPluginNames.insert(newPos, metadata.name());
                        }
                    }
                }

                if (indicatorPath.startsWith(QDir::homePath())) {
                    m_customLocalPluginIds << metadata.pluginId();
                }
            }

            qDebug() << " Indicator Package Loaded ::: " << metadata.name() << " [" << metadata.pluginId() << "]" << " - [" << indicatorPath <<"]";

            /*qDebug() << " Indicator value ::: " << metadata.pluginId();
                            qDebug() << " Indicator value ::: " << metadata.fileName();
                            qDebug() << " Indicator value ::: " << metadata.value("X-Latte-MainScript");
                            qDebug() << " Indicator value ::: " << metadata.value("X-Latte-ConfigUi");
                            qDebug() << " Indicator value ::: " << metadata.value("X-Latte-ConfigXml");*/
        }
    }

    if (!pluginChangedId.isEmpty()) {
        Q_EMIT indicatorChanged(pluginChangedId);
    }
}

void Factory::discoverNewIndicators(const QString &main)
{
    if (!m_mainPaths.contains(main)) {
        return;
    }

    QDirIterator indicatorsDirs(main, QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags);

    while(indicatorsDirs.hasNext()){
        indicatorsDirs.next();
        QString iPath = indicatorsDirs.filePath();

        if (!m_indicatorsPaths.contains(iPath)) {
            m_indicatorsPaths << iPath;
            KDirWatch::self()->addDir(iPath);
            reload(iPath);
        }
    }
}

void Factory::removeIndicatorRecords(const QString &path)
{
    if (m_indicatorsPaths.contains(path)) {
        QString pluginId =  path.section(QLatin1Char('/'), -1);
        m_plugins.remove(pluginId);
        m_pluginUiPaths.remove(pluginId);

        int pos = m_customPluginIds.indexOf(pluginId);

        if (pos >= 0) {
            m_customPluginIds.removeAt(pos);
            m_customPluginNames.removeAt(pos);
        }
        m_customLocalPluginIds.removeAll(pluginId);

        m_indicatorsPaths.removeAll(path);

        KDirWatch::self()->removeDir(path);

        //! delay informing the removal in case it is just an update
        QTimer::singleShot(1000, [this, pluginId]() {
            Q_EMIT indicatorRemoved(pluginId);
        });
    }
}

bool Factory::isCustomType(const QString &id) const
{
    return ((id != QLatin1String("org.kde.latte.default")) && (id != QLatin1String("org.kde.latte.plasma")) && (id != QLatin1String("org.kde.latte.plasmatabstyle")));
}

bool Factory::metadataAreValid(KPluginMetaData &metadata)
{
    return metadata.isValid()
            && pluginIdIsSafe(metadata.pluginId())
            && metadata.category() == QLatin1String("Latte Indicator")
            && !metadata.value(QStringLiteral("X-Latte-MainScript")).isEmpty();
}

bool Factory::metadataAreValid(QString &file)
{
    if (QFileInfo(file).exists()) {
        KPluginMetaData metadata = KPluginMetaData::fromJsonFile(file);
        return metadata.isValid();
    }

    return false;
}

QString Factory::uiPath(QString pluginName) const
{
    if (!m_pluginUiPaths.contains(pluginName)) {
        return QString();
    }

    return m_pluginUiPaths[pluginName];
}

QString Factory::metadataFileAbsolutePath(const QString &directoryPath)
{
    QString metadataFile = directoryPath + QStringLiteral("/metadata.json");

    if(QFileInfo(metadataFile).exists()) {
        return metadataFile;
    }

    metadataFile = directoryPath + QStringLiteral("/metadata.desktop");

    if(QFileInfo(metadataFile).exists()) {
        return metadataFile;
    }

    return QString();
}

Latte::ImportExport::State Factory::importIndicatorFile(QString compressedFile)
{
    auto showNotificationError = []() {
        auto notification = new KNotification(QStringLiteral("import-fail"), KNotification::CloseOnTimeout);
        notification->setText(i18n("Failed to import indicator"));
        notification->sendEvent();
    };

    auto showNotificationSucceed = [](QString name, bool updated) {
        auto notification = new KNotification(QStringLiteral("import-done"), KNotification::CloseOnTimeout);
        notification->setText(updated ? i18nc("indicator_name, imported updated","%1 indicator updated successfully", name) :
                                        i18nc("indicator_name, imported success","%1 indicator installed successfully", name));
        notification->sendEvent();
    };

    std::unique_ptr<KArchive> archive;

    auto zipArchive = std::make_unique<KZip>(compressedFile);

    //! KArchive::isOpen() stays true after a failed open(), so only the return value
    //! tells a zip apart from anything else
    if (zipArchive->open(QIODevice::ReadOnly)) {
        archive = std::move(zipArchive);
    } else {
        zipArchive.reset();

        auto tarArchive = std::make_unique<KTar>(compressedFile, QStringLiteral("application/x-tar"));

        if (!tarArchive->open(QIODevice::ReadOnly)) {
            showNotificationError();
            return Latte::ImportExport::FailedState;
        }

        archive = std::move(tarArchive);
    }

    QTemporaryDir archiveTempDir;

    //! an invalid QTemporaryDir has an empty path(), and copyTo("") would unpack the
    //! archive into the working directory
    if (!archiveTempDir.isValid()) {
        qWarning() << "Indicator import failed, no usable temporary directory ::" << compressedFile;
        showNotificationError();
        return Latte::ImportExport::FailedState;
    }

    if (!archive->directory()->copyTo(archiveTempDir.path())) {
        qWarning() << "Indicator import failed, archive could not be extracted ::" << compressedFile;
        showNotificationError();
        return Latte::ImportExport::FailedState;
    }

    archive->close();

    //metadata file
    QString packagePath = archiveTempDir.path();
    QString metadataFile = metadataFileAbsolutePath(archiveTempDir.path());

    if (!QFileInfo(metadataFile).exists()){
        QDirIterator iter(archiveTempDir.path(), QDir::Dirs | QDir::NoDotAndDotDot);

        while(iter.hasNext() ) {
            QString currentPath = iter.next();

            QString tempMetadata = metadataFileAbsolutePath(currentPath);

            if (QFileInfo(tempMetadata).exists()) {
                metadataFile = tempMetadata;
                packagePath = currentPath;
            }
        }
    }

    KPluginMetaData metadata = KPluginMetaData::fromJsonFile(metadataFile);

    if (metadataAreValid(metadata)) {
        QStringList standardPaths = Latte::Layouts::Importer::standardPaths();

        if (standardPaths.isEmpty()) {
            showNotificationError();
            return Latte::ImportExport::FailedState;
        }

        const QString indicatorsPath = QDir::cleanPath(standardPaths.at(0) + QStringLiteral("/latte/indicators"));
        const QString installPath = QDir::cleanPath(indicatorsPath + QLatin1Char('/') + metadata.pluginId());

        //! the archive picks its own install directory through the metadata id, so an id
        //! such as "../../../.config/autostart" would have us wipe and replace a directory
        //! that has nothing to do with indicators
        if (!installPath.startsWith(indicatorsPath + QLatin1Char('/'))) {
            qWarning() << "Refusing to install indicator outside" << indicatorsPath << "::" << metadata.pluginId();
            showNotificationError();
            return Latte::ImportExport::FailedState;
        }

        bool updated{QDir(installPath).exists()};

        if (updated) {
            QDir(installPath).removeRecursively();
        }

        //! nothing else creates the indicators directory, so without this the first
        //! import on a profile that never downloaded one has nowhere to move to
        QDir().mkpath(indicatorsPath);

        QProcess process;
        process.start(QStringLiteral("mv"), {packagePath, installPath});

        //! exitCode() is 0 when the process never started at all
        if (!process.waitForFinished() || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            showNotificationError();
            return Latte::ImportExport::FailedState;
        }

        showNotificationSucceed(metadata.name(), updated);
        return Latte::ImportExport::InstalledState;
    }

    showNotificationError();
    return Latte::ImportExport::FailedState;
}

void Factory::removeIndicator(QString id)
{
    if (m_plugins.contains(id)) {
        QString pluginName = m_plugins[id].name();

        QDialog* dialog = new QDialog(nullptr);
        dialog->setWindowTitle(i18n("Remove Indicator Confirmation"));
        dialog->setObjectName(QStringLiteral("warning"));
        dialog->setAttribute(Qt::WA_DeleteOnClose);

        auto buttonbox = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::No);

        KMessageBox::createKMessageBox(dialog,
                                       buttonbox,
                                       QMessageBox::Question,
                                       i18n("Do you want to remove completely <b>%1</b> indicator from your system?", pluginName),
                                       QStringList(),
                                       QString(),
                                       0,
                                       KMessageBox::NoExec,
                                       QString());

        connect(buttonbox, &QDialogButtonBox::accepted, [&, id, pluginName]() {
            auto showRemovedSucceed = [](QString name) {
                auto notification = new KNotification(QStringLiteral("remove-done"), KNotification::CloseOnTimeout);
                notification->setText(i18nc("indicator_name, removed success","<b>%1</b> indicator removed successfully", name));
                notification->sendEvent();
            };

            qDebug() << "Trying to remove indicator :: " << id;
            QProcess process;
            process.start(QStringLiteral("kpackagetool6"), {QStringLiteral("-r"), id, QStringLiteral("-t"), QStringLiteral("Latte/Indicator")});
            process.waitForFinished();

            if (process.exitCode() == 0) {
                showRemovedSucceed(pluginName);
            }
        });

        dialog->show();
    }
}

void Factory::downloadIndicator()
{
    KNSWidgets::Dialog dialog(QStringLiteral("latte-indicators.knsrc"), m_parentWidget);
    dialog.exec();
}

}
}
