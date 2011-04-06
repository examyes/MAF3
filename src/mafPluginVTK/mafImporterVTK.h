/*
 *  mafImporterVTK.h
 *  mafPluginVTK
 *
 *  Created by Paolo Quadrani on 04/04/11.
 *  Copyright 2011 B3C. All rights reserved.
 *
 *  See Licence at: http://tiny.cc/QXJ4D
 *
 */

#ifndef MAFIMPORTERVTK_H
#define MAFIMPORTERVTK_H

// Includes list
#include "mafPluginVTKDefinitions.h"
#include <mafImporter.h>

namespace mafPluginVTK {

/**
 Class Name: mafImporterVTK
 This class allows to import VTK data into a mafVME through the serialization module.
 */
class MAFPLUGINVTKSHARED_EXPORT mafImporterVTK : public mafResources::mafImporter {
    Q_OBJECT
    /// typedef macro.
    mafSuperclassMacro(mafResources::mafImporter);
    
public:
    /// Object constructor.
    mafImporterVTK(const QString code_location = "");
    
public slots:
    /// Execute the resource algorithm.
    /*virtual*/ void execute();
    
protected:
    /// Object destructor.
    /* virtual */ ~mafImporterVTK();
};

} // namespace mafPluginVTK


#endif // MAFIMPORTERVTK_H