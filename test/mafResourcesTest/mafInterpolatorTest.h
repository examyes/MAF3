/*
 *  mafInterpolatorTest.cpp
 *  mafResourcesTest
 *
 *  Created by Daniele Giunchi on 14/07/14.
 *  Copyright 2009 SCS-B3C. All rights reserved.
 *
 *  See Licence at: http://tiny.cc/QXJ4D
 *
 */

#include <mafTestSuite.h>
#include <mafResourcesRegistration.h>
#include <mafCoreSingletons.h>
#include <mafInterpolator.h>

//forward declarations
 class  testInterpolatorCustom;

/**
 Class name: mafInterpolatorTest
 This class implements the test suite for mafInterpolator.
 */
class mafInterpolatorTest: public QObject {
    Q_OBJECT

private Q_SLOTS:
    /// Initialize test variables
    void initTestCase();

    /// Cleanup test variables memory allocation.
    void cleanupTestCase();

    /// mafInterpolatorBefore allocation test case.
    void mafInterpolatorAllocationTest();
    /// Test the interpolation strategy.
    void mafInterpolationStrategyTest();

private:
    testInterpolatorCustom *m_Interpolator; ///< Test var.
    mafResources::mafDataSetMap m_ItemMap; ///< Test data collection.
};