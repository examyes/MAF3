/*
 *  mafEventDispatcher.h
 *  mafEventBus
 *
 *  Created by Paolo Quadrani on 27/03/09.
 *  Copyright 2009 B3C. All rights reserved.
 *
 *  See Licence at: http://tiny.cc/QXJ4D
 *
 */

#ifndef MAFEVENTDISPATCHER_H
#define MAFEVENTDISPATCHER_H

#include "mafEventDefinitions.h"

namespace mafEventBus {

/**
 Class name: mafEventDispatcher
 This allows dispatching events coming from local application to attached observers.
 */
class MAFEVENTBUSSHARED_EXPORT mafEventDispatcher : public QObject {
    Q_OBJECT

public:
    /// object constructor.
    mafEventDispatcher();

    /// object destructor.
    /*virtual*/ ~mafEventDispatcher();

    /// Add the observer to the events.
    /** Return true if observer has beed added correctly, false otherwise.
    This method check before adding a new observer that it has not already been inserted into the events' Hash with the same id and callback signature.*/
    bool addObserver(const mafEvent &props);

    /// remove the callback from the observer's hash.
    bool removeObserver(const mafEvent &props);

    /// register custom signals use by objects to raise them events.
    /** Return true if signal has beed added correctly, false otherwise.
    This method check before adding a new signal that it has not already been inserted into the events' Hash with the same id and signal signature.
    WARNING: due to Qt limitation you cannot use the same signal in different ID.*/
    bool registerSignal(const mafEvent &props);

    /// Remove the signal from the signal's hash.
    bool removeSignal(const mafEvent &props);

    /// method used to check if the given signal has been already registered for the given id.
    bool isSignalPresent(const mafString topic) const;

    /// method used to check if the given signal has been already registered for the given id.
    //bool isSignalPresent(const mafString &id_name) const;

    /// Emit event corresponding to the given id (present into the event_dictionary) locally to the application.
    virtual void notifyEvent(const mafEvent &event_dictionary, mafEventArgumentsList *argList = NULL, mafGenericReturnArgument *returnArg = NULL) const;

signals:
    /// Default notification signals for default events.
    void notifyDefaultEvent();

    /// Signal used to notify to observers that the remote communication has been terminated with success.
    void remoteCommunicationDone();

    /// Signal used to notify to observers that the remote communication failed.
    void remoteCommunicationFailed();

protected:
    /// Register MAF global events
    virtual void initializeGlobalEvents();

    /// Interanl method used to remove the given event property.
    bool removeEventItem(const mafEvent &props);

    /// Return the signal item property associated to the given ID.
    mafEventItemListType signalItemProperty(const mafString topic) const;

private:
    /// method used to check if the given object has been already registered for the given id and signature.
    bool isSignaturePresent(const mafEvent &props) const;

    /// disconnection signal/observer.
    /** This function disconnects the signal and all the observers. */
    bool disconnectSignal(const mafEvent &props);

    /// This function disconnects observer from signal.
    bool disconnectCallback(const mafEvent &props);

    mafEventsHashType m_CallbacksHash; ///< Callbacks' hash for receiving events like updates or refreshes.
    mafEventsHashType m_SignalsHash; ///< Signals' hash for sending events.
};

/////////////////////////////////////////////////////////////
// Inline methods
/////////////////////////////////////////////////////////////

inline mafEventItemListType mafEventDispatcher::signalItemProperty(const mafString topic) const {
    return m_SignalsHash.values(topic);
}

inline bool mafEventDispatcher::isSignalPresent(const mafString topic) const {
    return m_SignalsHash.contains(topic);
}

} // namespace mafEventBus

#endif // MAFEVENTDISPATCHER_H