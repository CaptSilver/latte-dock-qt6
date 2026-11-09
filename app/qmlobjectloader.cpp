/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "qmlobjectloader.h"

#include "libplasmaapi.h"

// Qt
#include <QDebug>
#include <QQmlComponent>
#include <QQmlContext>

// KDE
#include <KLocalizedQmlContext>
#if LATTE_LIBPLASMA_6_8_API
#include <PlasmaQuick/PlasmaQuick>
#else
#include <PlasmaQuick/SharedQmlEngine>
#endif

namespace Latte {

//! The engine the dock's own QML runs on: the one libplasma shares process-wide.
static std::shared_ptr<QQmlEngine> plasmaQuickEngine()
{
#if LATTE_LIBPLASMA_6_8_API
    return PlasmaQuick::globalEngine();
#else
    //! Older libplasma hands the shared engine out through SharedQmlEngine, which holds it
    //! with a weak pointer. The throwaway object is only the way to get at it; the
    //! returned shared_ptr keeps the engine alive after the object is gone.
    PlasmaQuick::SharedQmlEngine sharedEngine;
    return sharedEngine.engine();
#endif
}

QmlObjectLoader::QmlObjectLoader(QObject *parent)
    : QmlObjectLoader(plasmaQuickEngine(), parent)
{
}

QmlObjectLoader::QmlObjectLoader(std::shared_ptr<QQmlEngine> engine, QObject *parent)
    : QObject(parent),
      m_engine(std::move(engine))
{
    m_context = new QQmlContext(m_engine.get(), this);

    m_localization = new KLocalizedQmlContext(m_context);
    m_context->setContextObject(m_localization);
    QQmlEngine::setContextForObject(m_localization, m_context);
}

QmlObjectLoader::~QmlObjectLoader()
{
    deleteRootObject();
}

QQmlContext *QmlObjectLoader::rootContext() const
{
    return m_context;
}

QQmlEngine *QmlObjectLoader::engine() const
{
    return m_engine.get();
}

QObject *QmlObjectLoader::rootObject() const
{
    return m_rootObject;
}

void QmlObjectLoader::setTranslationDomain(const QString &domain)
{
    m_localization->setTranslationDomain(domain);
}

bool QmlObjectLoader::deleteLaterWhenHidden()
{
    if (!m_rootObject || m_rootObject->metaObject()->indexOfSignal("visibleChanged(bool)") < 0) {
        return false;
    }

    connect(m_rootObject.data(), SIGNAL(visibleChanged(bool)), this, SLOT(onRootVisibleChanged(bool)));
    return true;
}

void QmlObjectLoader::onRootVisibleChanged(bool visible)
{
    if (!visible) {
        deleteLater();
    }
}

void QmlObjectLoader::deleteRootObject()
{
    //! The root has no parent: the component hands it over with C++ ownership.
    delete m_rootObject.data();
    m_rootObject.clear();
}

bool QmlObjectLoader::load(const QUrl &source, const QVariantMap &initialProperties)
{
    deleteRootObject();

    QQmlComponent component(m_engine.get(), source);

    if (component.isError()) {
        qWarning() << "QmlObjectLoader:" << source.toString() << "does not compile:" << component.errorString();
        return false;
    }

    QObject *root = component.beginCreate(m_context);

    if (!root) {
        qWarning() << "QmlObjectLoader:" << source.toString() << "could not be created:" << component.errorString();
        return false;
    }

    component.setInitialProperties(root, initialProperties);
    component.completeCreate();

    if (component.isError()) {
        qWarning() << "QmlObjectLoader:" << source.toString() << "failed while being created:" << component.errorString();
        delete root;
        return false;
    }

    m_rootObject = root;
    return true;
}

}
