/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef QMLOBJECTLOADER_H
#define QMLOBJECTLOADER_H

// Qt
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantMap>

// C++
#include <memory>

class KLocalizedQmlContext;
class QQmlContext;

namespace Latte {

//! Creates one QML file in a context of its own on a shared engine and owns the
//! resulting root object. libplasma 6.8 dropped SharedQmlEngine, which did this; this
//! keeps the part Latte uses: context properties and a translation domain that are
//! set before the file is created, initial properties, and a warning that names the
//! file when it does not compile.
class QmlObjectLoader : public QObject
{
    Q_OBJECT

public:
    //! Loads on the engine libplasma shares process-wide (PlasmaQuick::globalEngine(), or
    //! the one SharedQmlEngine hands out on older libplasma), the engine the dock's own QML runs on.
    explicit QmlObjectLoader(QObject *parent = nullptr);
    explicit QmlObjectLoader(std::shared_ptr<QQmlEngine> engine, QObject *parent = nullptr);
    ~QmlObjectLoader() override;

    //! Set properties on it before load(); the file reads them while it is created.
    QQmlContext *rootContext() const;

    //! Domain that i18n() in the loaded file translates against.
    void setTranslationDomain(const QString &domain);

    //! Creates the file, replacing whatever an earlier load() made. Returns false and
    //! leaves rootObject() null when it does not compile.
    bool load(const QUrl &source, const QVariantMap &initialProperties = QVariantMap());

    //! Null until a load() succeeded.
    QObject *rootObject() const;

    QQmlEngine *engine() const;

    //! Deletes the loader, and with it the root object, once the root reports that it
    //! was hidden. For a dialog that nobody holds on to. Returns false when there is no
    //! root or it has no visibleChanged signal.
    bool deleteLaterWhenHidden();

private Q_SLOTS:
    void onRootVisibleChanged(bool visible);

private:
    void deleteRootObject();

    std::shared_ptr<QQmlEngine> m_engine;
    QQmlContext *m_context{nullptr};
    KLocalizedQmlContext *m_localization{nullptr};
    QPointer<QObject> m_rootObject;
};

}

#endif
