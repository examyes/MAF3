/*
 *  mafOperation.h
 *  mafResources
 *
 *  Created by Paolo Quadrani on 30/12/09.
 *  Copyright 2009 B3C. All rights reserved.
 *
 *  See Licence at: http://tiny.cc/QXJ4D
 *
 */

#ifndef MAFOPERATION_H
#define MAFOPERATION_H

#include "mafResource.h"

namespace mafResources {

// Class forwarding list

/**
Class name: mafOperation
This class provides basic API for building elaboration algorithms for mafResources.
An operation takes as input one or more mafVMEs and generate as output a mafVME. The algorithm executed on the input data
is provided ad mafDataPipe. The mafOperation that manage the possibility to have the undo mechanism for the executed algorithm.
*/
class MAFRESOURCESSHARED_EXPORT mafOperation : public mafResource {
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning)
    Q_PROPERTY(bool executeOnThread READ executeOnThread WRITE setExecuteOnThread)
    Q_PROPERTY(bool canAbort READ canAbort)

    /// typedef macro.
    mafSuperclassMacro(mafResources::mafResource);

public:
    /// Object constructor.
    mafOperation(const QString code_location = "");

    /// Return true or false according to the unDo ability of the operation.
    bool canUnDo() const;

    /// Return the operation type (can be: mafOperationTypeImporter, mafOperationTypeExporter or mafOperationTypeOperation).
    mafOperationType operationType() const;

    /// check if the operation is running.
    bool isRunning() const;

    /// Initialize the operation. Put here the initialization of operation's parameters
    virtual bool initialize();

    /// Return the status of the execute on separate thread.
    bool executeOnThread() const;

    /// Return the abort capability of the operation.
    bool canAbort() const;

signals:
    /// Trigger the undo execution.
    void undoExecution();

public slots:
    /// Set the flag to alert the Operation Manager to move the operation's execution on a separate thread.
    void setExecuteOnThread(bool on_thread);

    /// Set parameters of operation.
    virtual void setParameters(QVariantList parameters);

    /// Allows to call the piece of algorithm that is needed to restore the previous state of the operation's execution.
    virtual void unDo();

    /// Allows to call the piece of algorithm that is needed to apply the operation again.
    virtual void reDo();

    /// Terminate the execution.
    virtual void terminate();

private slots:
    /// Terminate the execution.
    void abort();

protected:
    /// Object destructor.
    /* virtual */ ~mafOperation();

    mutable QMutex mutex;
    bool m_IsRunning; ///< Flag that check if the operation is running, i.e. the execution is started
    bool m_CanUnDo; ///< Flag that store the unDo capability of the operation.
    bool m_ExecuteOnThread; ///< Flag used to enable/disable the execution on separate thread.
    mafOperationType m_OperationType; ///< Describe the operation type (mafOperationTypeImporter, mafOperationTypeExporter or mafOperationTypeOperation).
    bool m_CanAbort;            ///< Flag indicating that the operation can abort its execution or no.
    bool m_Abort;                  ///< Flag indicating that the operation has to be aborted. The code inside the execute slot has to take care about it.
};

/////////////////////////////////////////////////////////////
// Inline methods
/////////////////////////////////////////////////////////////
inline bool mafOperation::isRunning() const {
    return m_IsRunning;
}

inline bool mafOperation::canUnDo() const {
    return m_CanUnDo;
}

inline mafOperationType mafOperation::operationType() const {
    return m_OperationType;
}

inline bool mafOperation::executeOnThread() const {
    return m_ExecuteOnThread;
}

inline void mafOperation::setExecuteOnThread(bool on_thread) {
    m_ExecuteOnThread = on_thread;
}

inline bool mafOperation::canAbort() const {
    return m_CanAbort;
}

} // namespace mafResources

#endif // MAFOPERATION_H
