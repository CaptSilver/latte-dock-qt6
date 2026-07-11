/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef STORAGEVALIDATOR_H
#define STORAGEVALIDATOR_H

// local
#include "../data/appletdata.h"
#include "../data/errordata.h"
#include "../data/viewstable.h"

// C++
#include <functional>

// Qt
#include <QList>
#include <QString>

class KConfigGroup;

namespace Latte {
namespace Layouts {

//! Pure layout-integrity checks lifted out of Storage so the live (active) and
//! on-disk (inactive) paths share one implementation and the detection logic is
//! directly unit-testable. A Storage method reads its source into a LayoutModel,
//! then delegates here. Iteration order and row-id sequencing mirror the original
//! methods exactly.
namespace StorageValidator {

//! mirrors Storage::IDNULL
constexpr int IDNULL = -1;

struct AppletModel
{
    QString id;
    QString pluginId;
    int subContainmentId = IDNULL; //! IDNULL when the applet is not a subcontainment host
};

struct ContainmentModel
{
    QString id;
    QString pluginId;
    bool isLatte = false; //! populated for parity with the source; no detector reads it yet
    QList<AppletModel> applets;
};

struct LayoutModel
{
    QList<ContainmentModel> containments;
};

//! Resolves a plugin id to Data::Applet metadata. Injected so the validator never
//! pulls in KPackage; Storage passes its metadata() method.
using MetadataResolver = std::function<Data::Applet(const QString &pluginId)>;

//! Builds a model from a layout file's "Containments" group. subIdOfApplet resolves
//! an applet group to its subcontainment id (Storage::subContainmentId), injected to
//! reuse the already-covered identity matching.
LayoutModel buildFromConfig(const KConfigGroup &containments,
                            const std::function<int(const KConfigGroup &)> &subIdOfApplet);

//! error/warning arrive with their id/name already set by the caller; these append
//! information rows and return whether any were found.
bool differentAppletsWithSameId(const LayoutModel &model, const MetadataResolver &resolve, Data::Error &error);
bool appletsAndContainmentsWithSameId(const LayoutModel &model, const MetadataResolver &resolve, Data::Warning &warning);
bool orphanedParentApplets(const LayoutModel &model, const MetadataResolver &resolve, Data::Error &error);
bool orphanedSubcontainments(const LayoutModel &model, const Data::ViewsTable &views, const MetadataResolver &resolve, Data::Warning &warning);

}
}
}

#endif
