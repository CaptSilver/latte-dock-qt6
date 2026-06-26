/*
    SPDX-FileCopyrightText: 2020 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "templatesmanager.h"

// local
#include "../layout/abstractlayout.h"
#include "../layout/centrallayout.h"
#include "../layouts/importer.h"
#include "../layouts/manager.h"
#include "../layouts/storage.h"
#include "../tools/commontools.h"
#include "../view/view.h"

// Qt
#include <QDir>
#include <QRegularExpression>

// KDE
#include <KDirWatch>
#include <KLocalizedString>

namespace Latte {
namespace Templates {

Manager::Manager(Latte::Corona *corona, QObject *parent)
    : QObject(parent),
      m_corona(corona)
{
    KDirWatch::self()->addDir(Latte::configPath() + QStringLiteral("/latte/templates"), KDirWatch::WatchFiles);
    connect(KDirWatch::self(), &KDirWatch::created, this, &Manager::onCustomTemplatesCountChanged);
    connect(KDirWatch::self(), &KDirWatch::deleted, this, &Manager::onCustomTemplatesCountChanged);
    connect(KDirWatch::self(), &KDirWatch::dirty, this, &Manager::onCustomTemplatesCountChanged);
}

Manager::~Manager()
{
}

void Manager::init()
{
    connect(this, &Manager::viewTemplatesChanged, m_corona->layoutsManager(), &Latte::Layouts::Manager::viewTemplatesChanged);

    initLayoutTemplates();
    initViewTemplates();
}

void Manager::initLayoutTemplates()
{
    m_layoutTemplates.clear();
    initLayoutTemplates(m_corona->kPackage().filePath("templates"));
    initLayoutTemplates(Latte::configPath() + QStringLiteral("/latte/templates"));
    Q_EMIT layoutTemplatesChanged();
}

void Manager::initViewTemplates()
{
    m_viewTemplates.clear();
    initViewTemplates(m_corona->kPackage().filePath("templates"));
    initViewTemplates(Latte::configPath() + QStringLiteral("/latte/templates"));
    Q_EMIT viewTemplatesChanged();
}

void Manager::initLayoutTemplates(const QString &path)
{
    QDir templatesDir(path);
    QStringList filter;
    filter.append(QStringLiteral("*.layout.latte"));
    QStringList templates = templatesDir.entryList(filter, QDir::Files | QDir::Hidden | QDir::NoSymLinks);

    for (int i=0; i<templates.count(); ++i) {
        QString templatePath = templatesDir.path() + QStringLiteral("/") + templates[i];
        if (!m_layoutTemplates.containsId(templatePath)) {
            CentralLayout layouttemplate(this, templatePath);

            Data::Layout tdata = layouttemplate.data();
            tdata.isTemplate = true;

            if (tdata.name == QLatin1String(DEFAULTLAYOUTTEMPLATENAME) || tdata.name == QLatin1String(EMPTYLAYOUTTEMPLATENAME)) {
                QByteArray templateNameChars = tdata.name.toUtf8();
                tdata.name = i18n(templateNameChars.constData());
            }

            m_layoutTemplates << tdata;
        }
    }
}

void Manager::initViewTemplates(const QString &path)
{
    bool istranslated = (m_corona->kPackage().filePath("templates") == path);

    QDir templatesDir(path);
    QStringList filter;
    filter.append(QStringLiteral("*.view.latte"));
    QStringList templates = templatesDir.entryList(filter, QDir::Files | QDir::Hidden | QDir::NoSymLinks);

    for (int i=0; i<templates.count(); ++i) {
        QString templatePath = templatesDir.path() + QStringLiteral("/") + templates[i];

        if (!m_viewTemplates.containsId(templatePath)) {
            Data::Generic vdata;
            vdata.id = templatePath;
            QString tname = QFileInfo(templatePath).baseName();

            if (istranslated) {
                QByteArray tnamechars = tname.toUtf8();
                vdata.name = i18nc("view template name", tnamechars.constData());
            } else {
                vdata.name = tname;
            }

            m_viewTemplates << vdata;
        }
    }
}

Data::Layout Manager::layoutTemplateForName(const QString &layoutName)
{
    if (m_layoutTemplates.containsName(layoutName)) {
        QString layoutid = m_layoutTemplates.idForName(layoutName);
        return m_layoutTemplates[layoutid];
    }

    return Data::Layout();
}

Data::LayoutsTable Manager::layoutTemplates()
{
    Data::LayoutsTable templates;

    QString id = m_layoutTemplates.idForName(i18n(DEFAULTLAYOUTTEMPLATENAME));
    templates << m_layoutTemplates[id];
    id = m_layoutTemplates.idForName(i18n(EMPTYLAYOUTTEMPLATENAME));
    templates << m_layoutTemplates[id];

    for (int i=0; i<m_layoutTemplates.rowCount(); ++i) {
        if ( m_layoutTemplates[i].name != i18n(DEFAULTLAYOUTTEMPLATENAME)
             && m_layoutTemplates[i].name != i18n(EMPTYLAYOUTTEMPLATENAME)
             && m_layoutTemplates[i].name != QLatin1String(Layout::MULTIPLELAYOUTSHIDDENNAME)) {
            templates << m_layoutTemplates[i];
        }
    }

    return templates;
}

Data::GenericBasicTable Manager::viewTemplates()
{
    return m_viewTemplates;
}

QString Manager::newLayout(QString layoutName, QString layoutTemplate)
{
    if (!m_layoutTemplates.containsName(layoutTemplate)) {
        return QString();
    }

    if (layoutName.isEmpty()) {
        layoutName = Layouts::Importer::uniqueLayoutName(layoutTemplate);
    } else {
        layoutName = Layouts::Importer::uniqueLayoutName(layoutName);
    }

    QString newLayoutPath = Layouts::Importer::layoutUserFilePath(layoutName);

    Data::Layout dlayout = layoutTemplateForName(layoutTemplate);
    QFile(dlayout.id).copy(newLayoutPath);
    qDebug() << "adding layout : " << layoutName << " based on layout template:" << layoutTemplate;

    Q_EMIT newLayoutAdded(newLayoutPath);

    return newLayoutPath;
}

bool Manager::exportTemplate(const QString &originFile, const QString &destinationFile, const Data::AppletsTable &approvedApplets)
{
    return Latte::Layouts::Storage::self()->exportTemplate(originFile, destinationFile, approvedApplets);
}

bool Manager::exportTemplate(const Latte::View *view, const QString &destinationFile, const Data::AppletsTable &approvedApplets)
{
    return Latte::Layouts::Storage::self()->exportTemplate(view->layout(), view->containment(), destinationFile, approvedApplets);
}

void Manager::onCustomTemplatesCountChanged(const QString &file)
{
    if (file.startsWith(Latte::configPath() + QStringLiteral("/latte/templates"))) {
        if (file.endsWith(QStringLiteral(".layout.latte"))) {
            initLayoutTemplates();
        } else if (file.endsWith(QStringLiteral(".view.latte"))) {
            initViewTemplates();
        }
    }
}

void Manager::importSystemLayouts()
{
    for (int i=0; i<m_layoutTemplates.rowCount(); ++i) {
        if (m_layoutTemplates[i].isSystemTemplate()) {
            QString userLayoutPath = Layouts::Importer::layoutUserFilePath(m_layoutTemplates[i].name);

            if (!QFile(userLayoutPath).exists()) {
                QFile(m_layoutTemplates[i].id).copy(userLayoutPath);
                qDebug() << "adding layout : " << userLayoutPath << " based on layout template:" << m_layoutTemplates[i].name;
            }
        }
    }
}

QString Manager::proposedTemplateAbsolutePath(QString templateFilename)
{
    QString tempfilename = templateFilename;

    if (tempfilename.endsWith(QStringLiteral(".layout.latte"))) {
        QString clearedname = tempfilename.chopped(QStringLiteral(".layout.latte").size());
        tempfilename = uniqueLayoutTemplateName(clearedname) + QStringLiteral(".layout.latte");
    } else if (tempfilename.endsWith(QStringLiteral(".view.latte"))) {
        QString clearedname = tempfilename.chopped(QStringLiteral(".view.latte").size());
        tempfilename = uniqueViewTemplateName(clearedname) + QStringLiteral(".view.latte");
    }

    return Latte::configPath() + QStringLiteral("/latte/templates/") + tempfilename;
}

bool Manager::hasCustomLayoutTemplate(const QString &templateName) const
{
    for (int i=0; i<m_layoutTemplates.rowCount(); ++i) {
        if (m_layoutTemplates[i].name == templateName && !m_layoutTemplates[i].isSystemTemplate()) {
            return true;
        }
    }

    return false;
}

bool Manager::hasLayoutTemplate(const QString &templateName) const
{
    return m_layoutTemplates.containsName(templateName);
}

bool Manager::hasViewTemplate(const QString &templateName) const
{
    return m_viewTemplates.containsName(templateName);
}

QString Manager::viewTemplateFilePath(const QString templateName) const
{
    if (m_viewTemplates.containsName(templateName)) {
        return m_viewTemplates.idForName(templateName);
    }

    return QString();
}

void Manager::installCustomLayoutTemplate(const QString &templateFilePath)
{
    if (!templateFilePath.endsWith(QStringLiteral(".layout.latte"))) {
        return;
    }

    QString layoutName = QFileInfo(templateFilePath).baseName();

    QString destinationFilePath = Latte::configPath() + QStringLiteral("/latte/templates/") + layoutName + QStringLiteral(".layout.latte");

    if (hasCustomLayoutTemplate(layoutName)) {
        QFile(destinationFilePath).remove();
    }

    QFile(templateFilePath).copy(destinationFilePath);
}

QString Manager::uniqueLayoutTemplateName(QString name) const
{
    int pos_ = name.lastIndexOf(QRegularExpression(QStringLiteral(" - [0-9]+")));

    if (hasLayoutTemplate(name) && pos_ > 0) {
        name = name.left(pos_);
    }

    int i = 2;

    QString namePart = name;

    while (hasLayoutTemplate(name)) {
        name = namePart + QStringLiteral(" - ") + QString::number(i);
        i++;
    }

    return name;
}

QString Manager::uniqueViewTemplateName(QString name) const
{
    int pos_ = name.lastIndexOf(QRegularExpression(QStringLiteral(" - [0-9]+")));

    if (hasViewTemplate(name) && pos_ > 0) {
        name = name.left(pos_);
    }

    int i = 2;

    QString namePart = name;

    while (hasViewTemplate(name)) {
        name = namePart + QStringLiteral(" - ") + QString::number(i);
        i++;
    }

    return name;
}

QString Manager::templateName(const QString &filePath)
{
    int lastSlash = filePath.lastIndexOf(QLatin1Char('/'));
    QString templatename = filePath.mid(lastSlash + 1);

    //! strip a recognised template extension; a name ending in neither is left
    //! untouched (remove() with a not-found -1 index would chop the last char).
    const QString extensions[] = {QStringLiteral(".layout.latte"), QStringLiteral(".view.latte")};
    for (const QString &extension : extensions) {
        if (templatename.endsWith(extension)) {
            templatename.chop(extension.size());
            break;
        }
    }

    return templatename;
}

//! it is used in order to provide translations for system templates
void Manager::exposeTranslatedTemplateNames()
{
    //! layout templates default names
    i18nc("default layout template name", "Default");
    i18nc("empty layout template name", "Empty");

    //! dock/panel templates default names
    i18nc("view template name", "Default Dock");
    i18nc("view template name", "Default Panel");
    i18nc("view template name", "Empty Panel");
}

}
}
