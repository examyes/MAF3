/*
 *  mafExporterVTK.cpp
 *  mafPluginVTK
 *
 *  Created by Daniele Giunchi on 04/04/11.
 *  Copyright 2011 B3C. All rights reserved.
 *
 *  See Licence at: http://tiny.cc/QXJ4D
 *
 */

#include "mafExporterVTK.h"

#include <vtkDataSetWriter.h>
#include <vtkAlgorithmOutput.h>
#include <mafProxy.h>

using namespace mafPluginVTK;
using namespace mafResources;
using namespace mafCore;

mafExporterVTK::mafExporterVTK(const QString code_location) : mafOperation(code_location), m_ExportAllTimestamps(false) {
    m_Writer = vtkDataSetWriter::New();
    setProperty("wildcard", mafTr("VTK Files (*.vtk)"));
}

mafExporterVTK::~mafExporterVTK() {
    m_Writer->Delete();
}

void mafExporterVTK::execute() {
    m_Status = mafOperationStatusExecuting;
    
    if (!filename().isEmpty()) {
        if (m_Status == mafOperationStatusAborted) {
            cleanup();
            return;
        }
    }
    mafResource *res = input();
    mafVME *vme = qobject_cast<mafResources::mafVME *>(res);
    
    mafDataSetCollection *dc = vme->dataSetCollection();
    mafDataSet *data = dc->itemAt(-1);
    mafProxyInterface *dv = data->dataValue();
    mafProxy<vtkAlgorithmOutput> *value = mafProxyPointerTypeCast(vtkAlgorithmOutput, dv);
    m_Writer->SetInputConnection(0, *value);
    m_Writer->SetFileName(filename().toAscii().constData());
    m_Writer->Update();
        
    emit executionEnded();
}

bool mafExporterVTK::acceptObject(mafCore::mafObjectBase *obj) {
    mafVME *vme = qobject_cast<mafResources::mafVME *>(obj);
    if (vme == NULL) {
        qCritical(mafTr("Missing VME!!").toAscii().constData());
        return false;
    }
    mafDataSetCollection *dc = vme->dataSetCollection();
    mafDataSet *ds = (*dc)[0];
    mafCore::mafProxyInterface *dv = ds->dataValue();
    QString dt("");
    if (dv) {
        dt = dv->externalDataType();
    }
    return dt.contains("vtk");
}

void mafExporterVTK::setParameters(QVariantList parameters) {
    m_Filename = parameters.at(0).toString();
}

void mafExporterVTK::terminated() {
    
}

void mafExporterVTK::cleanup() {
    // Cleanup memory and deregister callback.
    mafDEL(m_Output);
}

void mafExporterVTK::unDo() {
}

void mafExporterVTK::reDo() {
}

void mafExporterVTK::setFilename(const QString f) {
}

/// Return the filename of the VTK data to import.
QString mafExporterVTK::filename() const {
    return QString();
}

