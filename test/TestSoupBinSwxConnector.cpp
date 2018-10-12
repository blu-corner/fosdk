#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "gwcSwx.h"

using namespace neueda;
using namespace ::testing;


class SoupBinSwxTestHarness : public Test
{
protected:
    virtual void SetUp()
    {
        ::remove ("millennium.seqno.cache");
        
        mLogger = logService::getLogger ("TEST_LSE");
        mProps = new properties("gwc", "millennium", "sim");

        mSessionCallbacks = new MockSessionCallbacks();
        mMessageCallbacks = new MockMessageCallbacks();

        mConnector = new MockLseConnector(mLogger);
        mMockConnectionActive = false;
    }

    virtual void TearDown()
    {
        EXPECT_CALL (*mSessionCallbacks, onLoggedOff(_, 
                                                     _))
            .Times(1);
        
        mConnector->stop();

        if (mMockConnectionActive)
        {
            EXPECT_CALL(*mMessageCallbacks, onAdmin(_, _))
                .Times (1);
            mockLogoffReplyRealTime ();
        }

        delete mConnector;
        delete mSessionCallbacks;
        delete mMessageCallbacks;
        delete mProps;
    }

    logger* mLogger;
    properties* mProps;
    MockSessionCallbacks* mSessionCallbacks; 
    MockMessageCallbacks* mMessageCallbacks;
    MockSoupBinSwxConnector* mConnector;

    MockSbfTcpConnection* mMockConnection;
    SbfTcpConnectionDelegate* mConncetionDelegate;
    
    bool mMockConnectionActive;
};

// TESTS

TEST_F(SoupBinSwxTestHarness, TEST_THAT_INIT_FAILS_ON_MISSING_REALTIME_HOST_PARAM)
{
    bool ok = mConnector->init (mSessionCallbacks, mMessageCallbacks, *mProps);
    ASSERT_FALSE(ok);
}
