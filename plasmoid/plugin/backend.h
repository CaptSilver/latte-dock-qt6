/*
    SPDX-FileCopyrightText: 2013-2016 Eike Hein <hein@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KConfigWatcher>

#include <QObject>
#include <QRect>
#include <QUrl>
#include <QVariant>

#include <netwm.h>
#include <qwindowdefs.h>

#include "kactivitymanagerd_plugins_settings.h"

class QAction;
class QActionGroup;
class QDBusServiceWatcher;
class QQuickItem;
class QQuickWindow;
class QJsonArray;

namespace KActivities
{
class Consumer;
}

class Backend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QQuickItem *taskManagerItem READ taskManagerItem WRITE setTaskManagerItem NOTIFY taskManagerItemChanged)
    Q_PROPERTY(bool highlightWindows READ highlightWindows WRITE setHighlightWindows NOTIFY highlightWindowsChanged)
    Q_PROPERTY(bool windowViewAvailable READ windowViewAvailable NOTIFY windowViewAvailableChanged)

public:
    enum MiddleClickAction {
        None = 0,
        Close,
        NewInstance,
        ToggleMinimized,
        ToggleGrouping,
        BringToCurrentDesktop,
    };

    Q_ENUM(MiddleClickAction)

    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    Q_INVOKABLE QVariantList jumpListActions(const QUrl &launcherUrl, QObject *parent);
    Q_INVOKABLE QVariantList placesActions(const QUrl &launcherUrl, bool showAllPlaces, QObject *parent);
    Q_INVOKABLE QVariantList recentDocumentActions(const QUrl &launcherUrl, QObject *parent);
    Q_INVOKABLE void setActionGroup(QAction *action) const;

    Q_INVOKABLE QRect globalRect(QQuickItem *item) const;

    Q_INVOKABLE bool isApplication(const QUrl &url) const;

    Q_INVOKABLE qint64 parentPid(qint64 pid) const;

    Q_INVOKABLE static QUrl tryDecodeApplicationsUrl(const QUrl &launcherUrl);
    Q_INVOKABLE static QStringList applicationCategories(const QUrl &launcherUrl);

    QQuickItem *taskManagerItem() const;
    void setTaskManagerItem(QQuickItem *item);
    bool highlightWindows() const;
    void setHighlightWindows(bool highlight);

    Q_INVOKABLE QList<QUrl> jsonArrayToUrlList(const QJsonArray &array) const;
    Q_INVOKABLE static QVariantMap generateMimeData(const QString &mimeType, const QVariant &mimeData, const QUrl &url);
    Q_INVOKABLE void ungrabMouse(QQuickItem *item) const;

    bool windowViewAvailable() const;

    Q_INVOKABLE void activateWindowView(const QVariant &winIds);
    Q_INVOKABLE void windowsHovered(const QVariant &winIds, bool hovered);
    Q_INVOKABLE void cancelHighlightWindows();

Q_SIGNALS:
    void addLauncher(const QUrl &url) const;

    void showAllPlaces();

    void taskManagerItemChanged();
    void highlightWindowsChanged();
    void windowViewAvailableChanged();

private Q_SLOTS:
    void handleRecentDocumentAction() const;

private:
    QVariantList systemSettingsActions(QObject *parent) const;
    QStringList winIdsToStrings(const QVariant &winIds) const;

    QActionGroup *m_actionGroup = nullptr;
    KActivities::Consumer *m_activitiesConsumer = nullptr;

    KActivityManagerdPluginsSettings m_activityManagerPluginsSettings;
    KConfigWatcher::Ptr m_activityManagerPluginsSettingsWatcher;

    QQuickItem *m_taskManagerItem = nullptr;
    bool m_highlightWindows = false;

    QDBusServiceWatcher *m_windowViewWatcher = nullptr;
    bool m_windowViewAvailable = false;
};
