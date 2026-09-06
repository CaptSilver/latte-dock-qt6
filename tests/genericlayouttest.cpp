/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// GenericLayout's view-factory handle and its containment list, driven through CentralLayout,
// its only concrete subclass. A CentralLayout built from an empty file path is inert:
// AbstractLayout's constructor does nothing unless the layout file exists, and CentralLayout's
// gates on that same result, so the subject can be constructed and destroyed with no Corona
// behind it. That is what makes the factory handle reachable from a headless test at all --
// everything else on GenericLayout wants a live Corona.

#include "layout/centrallayout.h"
#include "layout/iviewfactory.h"

#include <QObject>
#include <QString>
#include <QtTest>

namespace {

//! Records its own destruction, so a test can prove the layout did not take ownership of an
//! injected factory. createView answers nullptr because no case here wants a real view.
class SpyFactory : public Latte::Layout::IViewFactory
{
public:
    explicit SpyFactory(bool *destroyed)
        : m_destroyed(destroyed)
    {
    }

    ~SpyFactory() override
    {
        *m_destroyed = true;
    }

    Latte::View *createView(Latte::Layout::GenericLayout *, const Latte::Layout::AddViewRequest &) override
    {
        return nullptr;
    }

private:
    bool *m_destroyed;
};

}

class GenericLayoutTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultFactoryIsStable();
    void injectedFactoryReplacesTheDefault();
    void injectedFactoryOutlivesTheLayout();
    void clearingTheFactoryRestoresADefault();
    void readoptingTheCurrentFactoryKeepsItUsable();
    void containmentsAreEmptyOnAnInertLayout();
};

void GenericLayoutTest::defaultFactoryIsStable()
{
    Latte::CentralLayout layout(nullptr, QString());

    Latte::Layout::IViewFactory *factory = layout.viewFactory();
    QVERIFY(factory);
    QCOMPARE(layout.viewFactory(), factory);
}

void GenericLayoutTest::injectedFactoryReplacesTheDefault()
{
    bool destroyed{false};
    SpyFactory spy(&destroyed);
    Latte::Layout::IViewFactory *injected = &spy;

    Latte::CentralLayout layout(nullptr, QString());
    QVERIFY(layout.viewFactory() != injected);

    layout.setViewFactory(injected);
    QCOMPARE(layout.viewFactory(), injected);
}

void GenericLayoutTest::injectedFactoryOutlivesTheLayout()
{
    bool destroyed{false};
    SpyFactory spy(&destroyed);

    {
        Latte::CentralLayout layout(nullptr, QString());
        //! ask for the default first, so the injection has something to displace
        layout.viewFactory();
        layout.setViewFactory(&spy);
    }

    QVERIFY2(!destroyed, "the layout destroyed a factory it was only lent");
}

void GenericLayoutTest::clearingTheFactoryRestoresADefault()
{
    bool destroyed{false};
    SpyFactory spy(&destroyed);
    Latte::Layout::IViewFactory *injected = &spy;

    Latte::CentralLayout layout(nullptr, QString());
    layout.setViewFactory(injected);
    layout.setViewFactory(nullptr);

    //! addView() calls viewFactory()->createView() unconditionally, so clearing the injection
    //! has to leave a usable factory rather than a null handle.
    Latte::Layout::IViewFactory *restored = layout.viewFactory();
    QVERIFY(restored);
    QVERIFY(restored != injected);
}

void GenericLayoutTest::readoptingTheCurrentFactoryKeepsItUsable()
{
    Latte::CentralLayout layout(nullptr, QString());

    Latte::Layout::IViewFactory *factory = layout.viewFactory();
    layout.setViewFactory(factory);
    QCOMPARE(layout.viewFactory(), factory);

    //! The clone branch bails out before it reads the corona or the containment, so a cloned
    //! request with no original is the one view a layout with neither can be asked to build.
    Latte::Layout::AddViewRequest request;
    request.isCloned = true;
    QVERIFY(layout.viewFactory()->createView(&layout, request) == nullptr);
}

void GenericLayoutTest::containmentsAreEmptyOnAnInertLayout()
{
    Latte::CentralLayout layout(nullptr, QString());

    //! Reads as a plain container, which is the point: this line does not compile while
    //! containments() hands out the address of the list. Reference versus value is invisible
    //! here -- both bind the same way -- and is pinned in sourceguardtest instead.
    QVERIFY(layout.containments().isEmpty());
}

QTEST_MAIN(GenericLayoutTest)
#include "genericlayouttest.moc"
