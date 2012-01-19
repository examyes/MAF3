/*
 *  mafPipeDataSliceVolume.h
 *  mafPluginVTK
 *
 *  Created by Paolo Quadrani on 14/11/11.
 *  Copyright 2011 B3C. All rights reserved.
 *
 *  See License at: http://tiny.cc/QXJ4D
 *
 */

#ifndef MAFPIPEDATASLICEVOLUME_H
#define MAFPIPEDATASLICEVOLUME_H

// Includes list
#include "mafPluginVTKDefinitions.h"
#include <mafPipeDataSlice.h>
#include <mafProxy.h>
#include <vtkSmartPointer.h>
#include <vtkAlgorithmOutput.h>
#include <vtkMath.h>

// Foundation Class forwarding list
class vtkMAFVolumeSlicer;
class vtkImageData;
class vtkPolyData;

namespace mafPluginVTK {

// Class forwarding list

/**
 Class name: mafPipeDataSliceVolume
 This class allows you to make a slice on input vtkPolyData.
 */
class MAFPLUGINVTKSHARED_EXPORT mafPipeDataSliceVolume : public mafResources::mafPipeDataSlice {
    Q_OBJECT

    /// typedef macro.
    mafSuperclassMacro(mafResources::mafPipeDataSlice);

public:
    /// Object constructor.
    mafPipeDataSliceVolume(const QString code_location = "");

    /// Accept function
    static bool acceptObject(mafCore::mafObjectBase *obj);

    /// Return the textured output generated by the elaboration algorithm.
    mafCore::mafProxyInterfacePointer textureOutput();

public Q_SLOTS:
    /// Allow to execute and update the pipeline when something change
    /*virtual*/ void updatePipe(double t = -1);

protected:
    /// Object destructor.
    /* virtual */ ~mafPipeDataSliceVolume();

private:
    vtkSmartPointer<vtkMAFVolumeSlicer> m_Slicer; ///< Volume slicer for the input volume data.
    mafCore::mafProxy<vtkAlgorithmOutput> m_OutputPolygonal; ///< Output value coming from the elaboration data pipe.
    mafCore::mafProxy<vtkAlgorithmOutput> m_OutputTexture; ///< Output texture coming from the elaboration data pipe.
};


/////////////////////////////////////////////////////////////
// Inline methods
/////////////////////////////////////////////////////////////

inline mafCore::mafProxyInterfacePointer mafPipeDataSliceVolume::textureOutput() {
    return &m_OutputTexture;
}

} // namespace mafPluginVTK

#endif // MAFPIPEDATASLICEVOLUME_H
