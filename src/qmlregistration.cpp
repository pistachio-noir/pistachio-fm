#include "Apps.h"
#include "FileOps.h"
#include "FolderModel.h"
#include "MimeSettings.h"
#include "Places.h"
#include "Preview.h"

#include <QtQml/qqml.h>
#include <QtQml/qqmlmoduleregistration.h>

static void registerTypes()
{
    qmlRegisterTypesAndRevisions<Apps>("Pistachio.Files", 1);
    qmlRegisterTypesAndRevisions<FileOps>("Pistachio.Files", 1);
    qmlRegisterTypesAndRevisions<FolderModel>("Pistachio.Files", 1);
    qmlRegisterTypesAndRevisions<MimeSettings>("Pistachio.Files", 1);
    qmlRegisterTypesAndRevisions<Places>("Pistachio.Files", 1);
    qmlRegisterTypesAndRevisions<Preview>("Pistachio.Files", 1);
    qmlRegisterModule("Pistachio.Files", 1, 0);
}

static const QQmlModuleRegistration registration("Pistachio.Files", registerTypes);
