/*
 *  mafReferenceCounted.h
 *  mafCore
 *  
 *  Created by Paolo Quadrani on 13/12/11.
 *  Copyright 2011 B3C. All rights reserved.
 *  
 *  See License at: http://tiny.cc/QXJ4D
 *  
 */

#ifndef MAFREFERENCECOUNTED_H
#define MAFREFERENCECOUNTED_H

// Includes list
#include "mafCore_global.h"


namespace mafCore {

// Class forwarding list


/**
 Class name: mafReferenceCounted
 This class defines the MAF3 base object and implements the reference count mechanism used by all the MAF3 objects.
 */
class MAFCORESHARED_EXPORT mafReferenceCounted : public QObject {
    Q_OBJECT
    /// typedef macro.
    mafSuperclassMacro(QObject);
    
Q_SIGNALS:
    /// Allows to increment the reference count.
    void incrementReference();
    
    /// Allows to decrement the reference count.
    void decreaseReference();

private Q_SLOTS:
    /// increment of 1 unit the reference count.
    void ref();

    /// delete the object.
    void deleteObject();

public:
    /// Object constructor.
    mafReferenceCounted();

    /// Allows to Q_EMIT the incrementReference in a thread safe way.
    void retain();
    
    /// Allows to decrease the reference count of the object.
    void release();
    
    /// return the reference count.
    int referenceCount() const;
    
    /// dump the description of the object (information, attributes, variables...)
    virtual void description() const;

protected:
    /// Object destructor.
    virtual ~mafReferenceCounted();

private:
    volatile int m_ReferenceCount; ///< Index containing the reference count.
};


/////////////////////////////////////////////////////////////
// Inline methods
/////////////////////////////////////////////////////////////

inline int mafReferenceCounted::referenceCount() const{
    return m_ReferenceCount;
}

} // namespace mafCore

#endif  // MAFREFERENCECOUNTED_H
