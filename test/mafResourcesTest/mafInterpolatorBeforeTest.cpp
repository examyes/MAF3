/*
 *  mafInterpolatorBeforeTest.cpp
 *  mafResourcesTest
 *
 *  Created by Paolo Quadrani - Daniele Giunchi on 22/09/09.
 *  Copyright 2011 SCS-B3C. All rights reserved.
 *
 *  See License at: http://tiny.cc/QXJ4D
 *
 */

#include "mafResourcesTestList.h"

using namespace mafCore;
using namespace mafResources;

void mafInterpolatorBeforeTest::initTestCase() {
    mafMessageHandler::instance()->installMessageHandler();

    //! <snippet>
    m_Interpolator = mafNEW(mafResources::mafInterpolatorBefore);
    //! </snippet>

    double ts[10] = {0.5, 1.5, 10.5, 11.0, 12.8, 23.9, 52.1, 53.0, 53.1, 53.2};
    for(int i = 0; i < 10; ++i) {
        mafDataSet *item = mafNEW(mafResources::mafDataSet);
        m_ItemMap.insert(ts[i], item);
        mafDEL(item);
    }
}

void mafInterpolatorBeforeTest::cleanupTestCase() {
    m_ItemMap.clear();
    mafDEL(m_Interpolator);
    mafEventBus::mafEventBusManager::instance()->shutdown();
    mafMessageHandler::instance()->shutdown();
}

void mafInterpolatorBeforeTest::mafInterpolatorBeforeAllocationTest() {
    QVERIFY(m_Interpolator != NULL);
}

void mafInterpolatorBeforeTest::mafInterpolationBeforeStrategyTest() {
    mafDataSet *item;

    // item returned is at a time before the begin of the map; NULL should be returned.
    item = m_Interpolator->itemAt(&m_ItemMap, 0.0);
    QVERIFY(item == NULL);

    // Retrieve the first element.
    //! <snippet>
    item = m_Interpolator->itemAt(&m_ItemMap, 0.5);
    //! </snippet>
    QVERIFY(item != NULL);

    // Try to ask an element at a time over the timestemp max; the last element hsould be returned.
    item = m_Interpolator->itemAt(&m_ItemMap, 100.5);
    QVERIFY(item != NULL);

    QBENCHMARK {
        item = m_Interpolator->itemAt(&m_ItemMap, 48.0);
    }

    QVERIFY(item != NULL);
}

#include "mafInterpolatorBeforeTest.moc"
