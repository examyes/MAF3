/*
 *  mafPluginTest.cpp
 *  mafResourcesTest
 *
 *  Created by Paolo Quadrani on 22/09/09.
 *  Copyright 2009 B3C. All rights reserved.
 *
 *  See Licence at: http://tiny.cc/QXJ4D
 *
 */

#include <mafTestSuite.h>
#include <mafCoreSingletons.h>
#include <mafResourcesRegistration.h>
#include <mafResourcesSingletons.h>
#include <mafEventBusManager.h>
#include <mafPlugin.h>

#ifdef WIN32
    #define TEST_LIBRARY_NAME "mafPluginTest_d.dll"
#else
    #ifdef __APPLE__
        #define TEST_LIBRARY_NAME "libmafPluginTest_debug.dylib"
    #else
        #define TEST_LIBRARY_NAME "libmafPluginTest_debug.so"
    #endif
#endif

using namespace mafCore;
using namespace mafEventBus;
using namespace mafResources;

/**
 Class name: mafPluginTest
 This class implements the test suite for mafPlugin.
 */
class mafPluginTest : public QObject {
    Q_OBJECT

private slots:
    /// Initialize test variables
    void initTestCase() {
        mafResourcesRegistration::registerResourcesObjects();
        mafEventBusManager::instance();
        mafResourcesSingletons::mafSingletonsInitialize();
        // Initialize the plug-in name with prefix and extension.
        m_PluginName.append(TEST_LIBRARY_NAME);
        m_Plugin = NULL;
    }

    /// Cleanup test variables memory allocation.
    void cleanupTestCase() {
        // Free allocated memory
        mafDEL(m_Plugin);

        // Shutdown event bus singleton and core singletons.
        mafResourcesSingletons::mafSingletonsShutdown();
        mafEventBusManager::instance()->shutdown();
    }

    /// mafPlugin allocation test case.
    void mafPluginAllocationTest();
    /// Plug in Registration and info
    void pluginRegistrationTest();

private:
    mafPlugin *m_Plugin; ///< Test var.
    mafString m_PluginName; ///< Test var.
};

void mafPluginTest::mafPluginAllocationTest() {
    // instantiate the plug in and load the library.
    m_Plugin = new mafPlugin(m_PluginName, mafCodeLocation);
    QVERIFY(m_Plugin != NULL);
}

void mafPluginTest::pluginRegistrationTest() {
    m_Plugin->registerPlugin();
    mafPluginInfo info = m_Plugin->pluginInfo();
    QVERIFY(info.m_Author.length() > 0);
}

MAF_REGISTER_TEST(mafPluginTest);
#include "mafPluginTest.moc"