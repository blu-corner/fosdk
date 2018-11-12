#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "gwcMillennium.h"
#include "TestUtils.h"

using namespace neueda;
using namespace ::testing;


class MockLseConnector : public gwcMillennium<lseCodec>
{
public:
    MockLseConnector (logger* log)
        : gwcMillennium<lseCodec> (log)
    { }

    MOCK_METHOD1 (start, bool(bool reset));
    
    MOCK_METHOD0 (reset, void());

    void setRealTimeConnection (SbfTcpConnection** connection)
    {
        if (mRealTimeConnection)
            delete mRealTimeConnection;
        mRealTimeConnection = *connection;
    }

    void setRecoveryConnection (SbfTcpConnection** connection)
    {
        if (mRecoveryConnection)
            delete mRecoveryConnection;
        mRecoveryConnection = *connection;
    }

    void mockRealTimeConnectionReady ()
    {
        mRealTimeConnectionDelegate.onReady ();
    }

    void mockRealTimeConnectionError ()
    {
        mRealTimeConnectionDelegate.onError ();
    }

    size_t mockRealTimeConnectionRead (void* data, size_t len)
    {
        return mRealTimeConnectionDelegate.onRead (data, len);
    }

    void mockRecoveryConnectionReady ()
    {
        mRecoveryConnectionDelegate.onReady ();
    }

    void mockRecoveryConnectionError ()
    {
        mRecoveryConnectionDelegate.onError ();
    }

    size_t mockRecoveryConnectionRead (void* data, size_t len)
    {
        return mRecoveryConnectionDelegate.onRead (data, len);
    }
};

class LseMillenniumTestHarness : public Test
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

    cdr setLogonReplyMessage (cdr d, int rejectCode, int dayCount)
    {
        d.setString (MessageType, GW_MILLENNIUM_LOGON_REPLY);
        d.setInteger (RejectCode, rejectCode);
        d.setInteger (PasswordExpiryDayCount, dayCount);

        return d;
    }

    cdr setLogoutMessage (cdr d, string reason )
    {
        d.setString (MessageType, GW_MILLENNIUM_LOGOUT);
        d.setString (Reason, reason);

        return d;
    }

    cdr setReject (cdr d, int rejectCode, string rejectReason, string rejectedMsgType, string clOrdId)
    {
        d.setString (MessageType, GW_MILLENNIUM_REJECT);
        d.setInteger (RejectCode, rejectCode);
        d.setString (RejectReason, rejectReason);
        d.setString (RejectedMessageType, rejectedMsgType);
        d.setString (ClientOrderID, clOrdId);

        return d;
    }

    cdr setExecutionReport (cdr d, string execType)
    {
        d.setString (MessageType, GW_MILLENNIUM_EXECUTION_REPORT);
        d.setInteger (AppID, 123);
        d.setInteger (SequenceNo, 1234);
        d.setString (ExecutionID, "mockExecID");
        d.setString (ClientOrderID, "123");
        d.setString (OrderID, "orderID");
        d.setString (ExecType, execType);
        d.setString (ExecutionReportRefID, "ReportRefId");
        d.setInteger (OrderStatus, 2);
        d.setInteger (OrderRejectCode, 0);
        d.setDouble (ExecutedPrice, 2500.00);
        d.setInteger (ExecutedQty, 3000);
        d.setInteger (LeavesQty, 1500);
        d.setInteger (WaiverFlags, 123);
        d.setInteger (DisplayQty, 3000);
        d.setInteger (InstrumentID, 12345);
        d.setInteger (RestatementReason, 3);
        d.setInteger (ReservedField2, 2);
        d.setInteger (Side, 1);
        d.setInteger (ReservedField3, 1234567);
        d.setString (CounterParty, "counter");
        d.setString (TradeLiquidityIndicator, "A");
        d.setInteger (TradeMatchID, 1234567);
        d.setInteger (TransactTimeSeconds, 2345678);
        d.setInteger (TransactTimeUsecs, 2345678);
        d.setString (ReservedField4, "reserved4");
        d.setInteger (TypeOfTrade, 0);
        d.setInteger (Capacity, 1);
        d.setString (ReservedField5, "reserved5");
        d.setString (PublicOrderID, "pubOrderID");
        d.setInteger (MinimumQuantity, 150);

        return d;
    }

    void mockInitilizeConnector ()
    {
        mProps->setProperty ("real_time_host", "127.0.0.1:9899");
        mProps->setProperty ("recovery_host", "127.0.0.1:10000");
        bool ok = mConnector->init (mSessionCallbacks,
                                    mMessageCallbacks,
                                    *mProps);
        ASSERT_TRUE(ok);


        setupMockRealTimeConnection ();
        setupMockRecoveryConnection ();
    }

    void setupMockRealTimeConnection ()
    {
        mRealTimeDelegate =
                new gwcMillenniumRealTimeConnectionDelegate<lseCodec>(mConnector);
        mMockRealTimeConnection = new MockSbfTcpConnection (
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            true,
            mRealTimeDelegate);
        mConnector->setRealTimeConnection ((SbfTcpConnection**)&mMockRealTimeConnection);

        EXPECT_CALL (*mMockRealTimeConnection, send (_, _))
            .Times (AnyNumber ());
        EXPECT_CALL (*mMockRealTimeConnection, connect ())
            .Times (AnyNumber ())
            .WillRepeatedly(Return(true));
    }

    void setupMockRecoveryConnection ()
    {       
        mRecoveryDelegate =
            new gwcMillenniumRecoveryConnectionDelegate<lseCodec>(mConnector);
        mMockRecoveryConnection = new MockSbfTcpConnection (
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            true,
            mRecoveryDelegate);
        mConnector->setRecoveryConnection ((SbfTcpConnection**)&mMockRecoveryConnection);

        EXPECT_CALL (*mMockRecoveryConnection, send (_, _))
            .Times (AnyNumber ());
        EXPECT_CALL (*mMockRecoveryConnection, connect ())
            .Times (AnyNumber ())
            .WillRepeatedly(Return(true));
    }

    void mockRealTimeMessage (cdr& msg)
    {
        lseCodec codec;

        char space[1024];
        size_t used;
        codecState state = codec.encode (msg, space, sizeof space, used);
        
        if (state != GW_CODEC_SUCCESS)
            std::cout << codec.getLastError () << std::endl;
        
        ASSERT_EQ(state, GW_CODEC_SUCCESS);
        
        mRealTimeDelegate->onRead (space, used);
    }

    void mockRecoveryMessage (cdr& msg)
    {
        lseCodec codec;

        char space[1024];
        size_t used;
        codecState state = codec.encode (msg, space, sizeof space, used);
        
        if (state != GW_CODEC_SUCCESS)
            std::cout << codec.getLastError () << std::endl;
        
        ASSERT_EQ(state, GW_CODEC_SUCCESS);
        
        mRecoveryDelegate->onRead (space, used);
    }

    void mockLogonReplyRealTime ()
    {
        cdr d;
        d = setLogonReplyMessage (d, 0, 0);
        
        mockRealTimeMessage (d);
    }

    void mockLogonReject ()
    {
        cdr d;
        d = setLogonReplyMessage (d, 1, 0);

        mockRealTimeMessage (d);
    }

    void mockLogoffReplyRealTime ()
    {
        cdr d;
        d = setLogoutMessage (d, "mock");
        
        mockRealTimeMessage (d);
    }

    void mockLogonReplyRecovery ()
    {
        cdr d;
        d = setLogonReplyMessage (d, 0, 0);
        
        mockRecoveryMessage (d);
    }

    void mockRejectMessageRealTime ()
    {
        cdr d;
        d = setReject (d, 1, "mock", "3", "1234");

        mockRealTimeMessage (d);
    }

    void mockExecutionMessageRealTime (string execType)
    {
        cdr d;
        d = setExecutionReport (d, execType);

        mockRealTimeMessage (d);
    }

    void mockFullInitilizedConnector ()
    {
        mockInitilizeConnector ();
        EXPECT_CALL(*mSessionCallbacks, onLoggingOn(_)).Times(2);
        
        mConnector->mockRealTimeConnectionReady ();        
        EXPECT_CALL(*mMessageCallbacks, onAdmin(_, _))
            .Times (1);
        mockLogonReplyRealTime ();

        mConnector->mockRecoveryConnectionReady ();
        EXPECT_CALL(*mMessageCallbacks, onAdmin(_, _))
            .Times (1);
        EXPECT_CALL(*mSessionCallbacks, onLoggedOn(_, _))
            .Times (1);
        
        mockLogonReplyRecovery ();
        mMockConnectionActive = true;
    }

    gwcOrder getMockNewOrder ()
    {
        gwcOrder order;
        
        order.setPrice (1234.45);
        order.setQty (1000);
        order.setTif (GWC_TIF_DAY);
        order.setSide (GWC_SIDE_BUY);
        order.setOrderType (GWC_ORDER_TYPE_LIMIT);
        order.setString (ClientOrderID, "myorder");
        order.setInteger (InstrumentID, 133215); // VOD.L
        order.setInteger (AutoCancel, 1);
        order.setString (TraderID, "TX1");
        order.setString (Account, "account");
        order.setInteger (ClearingAccount, 1);
        order.setInteger (FXMiFIDFlags, 0);
        order.setInteger (PartyRoleQualifiers, 0);
        order.setInteger (ExpireDateTime, 0);
        order.setInteger (DisplayQty, 0);
        order.setInteger (Capacity, 1);
        order.setInteger (OrderSubType, 0);
        order.setInteger (Anonymity, 0);
        order.setDouble (StopPrice, 0.0);
        order.setInteger (PassiveOnlyOrder, 0);
        order.setInteger (ClientID, 1234);
        order.setInteger (InvestmentDecisionMaker, 0);
        order.setInteger (MinimumQuantity, 0);
        order.setInteger (ExecutingTrader, 7676);

        return order;
    }

    logger* mLogger;
    properties* mProps;
    MockSessionCallbacks* mSessionCallbacks; 
    MockMessageCallbacks* mMessageCallbacks;
    MockLseConnector* mConnector;

    MockSbfTcpConnection* mMockRealTimeConnection;
    MockSbfTcpConnection* mMockRecoveryConnection;
    SbfTcpConnectionDelegate* mRealTimeDelegate;
    SbfTcpConnectionDelegate* mRecoveryDelegate;
    
    bool mMockConnectionActive;
};

// TESTS

TEST_F(LseMillenniumTestHarness, TEST_THAT_INIT_FAILS_ON_MISSING_REALTIME_HOST_PARAM)
{
    bool ok = mConnector->init(mSessionCallbacks, mMessageCallbacks, *mProps);
    ASSERT_FALSE(ok);

}
TEST_F(LseMillenniumTestHarness, TEST_THAT_INIT_FAILS_ON_INVALID_REALTIME_HOST_PARAM)
{
    mProps->setProperty ("real_time_host", "Test.");
    bool ok = mConnector->init(mSessionCallbacks, mMessageCallbacks, *mProps);
    ASSERT_FALSE(ok);
}

TEST_F(LseMillenniumTestHarness, TEST_THAT_INIT_FAILS_ON_MISSING_RECOVERY_HOST_PARAM)
{
    mProps->setProperty ("real_time_host", "127.0.0.1:9899");
    bool ok = mConnector->init(mSessionCallbacks, mMessageCallbacks, *mProps);
    ASSERT_FALSE(ok);
}

TEST_F(LseMillenniumTestHarness, TEST_THAT_INIT_FAILS_ON_INVALID_RECOVERY_HOST_PARAM)
{
    mProps->setProperty ("real_time_host", "127.0.0.1:9899");
    mProps->setProperty ("real_time_host", "Test.");
    bool ok = mConnector->init(mSessionCallbacks, mMessageCallbacks, *mProps);
    ASSERT_FALSE(ok);
}

TEST_F(LseMillenniumTestHarness, TEST_THAT_INIT_SUCCEEDS_ON_VALID_PARAMS)
{
    mProps->setProperty ("real_time_host", "127.0.0.1:9899");
    mProps->setProperty ("recovery_host", "127.0.0.1:10000");
    bool ok = mConnector->init(mSessionCallbacks, mMessageCallbacks, *mProps);
    ASSERT_TRUE(ok);

    // helps give change for sbf_queue to cleanup properly
    sleep(1);
}

TEST_F(LseMillenniumTestHarness, TEST_THAT_ON_CONNECTION_READY_ONLOGGINGON_IS_CALLED)
{
    // setup
    mockInitilizeConnector ();
    
    // test
    EXPECT_CALL(*mSessionCallbacks, onLoggingOn(_)).Times(1);
    mConnector->mockRealTimeConnectionReady ();
}

TEST_F(LseMillenniumTestHarness, TEST_THAT_ON_REALTIME_CONNECTION_READY_WITH_LOGON_REPLY_ON_ADMIN_IS_CALLED)
{
    // setup
    mockInitilizeConnector ();
    EXPECT_CALL(*mSessionCallbacks, onLoggingOn(_)).Times(1);
        
    mConnector->mockRealTimeConnectionReady ();        
    EXPECT_CALL(*mMessageCallbacks, onAdmin(_, _))
        .Times (1);
    mockLogonReplyRealTime ();
}

TEST_F(LseMillenniumTestHarness, TEST_THAT_ON_REALTIME_CONNECTION_READY_WITH_LOGON_REJECT_ON_ERROR_IS_CALLED)
{
    // setup
    mockInitilizeConnector ();
    EXPECT_CALL(*mSessionCallbacks, onLoggingOn(_)).Times(1);
    mConnector->mockRealTimeConnectionReady ();
    
    EXPECT_CALL(*mMessageCallbacks, onAdmin(_, _))
        .Times(1);
    EXPECT_CALL(*mSessionCallbacks, onError(_))
        .Times(1);
    mockLogonReject ();
}

TEST_F(LseMillenniumTestHarness, TEST_THAT_CANNOT_SEND_ORDER_IF_NOT_LOGGED_ON)
{
    // setup
    mockInitilizeConnector ();
    EXPECT_CALL(*mSessionCallbacks, onLoggingOn(_)).Times(1);
    
    mConnector->mockRealTimeConnectionReady ();

    // do test
    gwcOrder mockOrder = getMockNewOrder ();
    bool ok = mConnector->sendOrder (mockOrder);

    // check
    ASSERT_FALSE (ok);
}

TEST_F(LseMillenniumTestHarness, TEST_THAT_CAN_SEND_ORDER_IF_LOGGED_ON_BOTH_REAL_TIME_AND_RECOVERY)
{
    // setup
    mockFullInitilizedConnector ();
    
    // do test
    gwcOrder mockOrder = getMockNewOrder ();
    bool ok = mConnector->sendOrder (mockOrder);

    // check
    ASSERT_TRUE (ok);
}

TEST_F(LseMillenniumTestHarness, TEST_THAT_ON_REJECT_MESSAGE_ON_MSG_CALLBACK_IS_CALLED)
{
    // setup
    mockFullInitilizedConnector ();
    EXPECT_CALL(*mMessageCallbacks, onMsg(_, _)).Times(1);

    mockRejectMessageRealTime ();
}

TEST_F(LseMillenniumTestHarness, TEST_THAT_ON_EXECUTION_EXEC_TYPE_ZERO_ON_ORDER_ACK_CALLBACK_IS_CALLED)
{
    // setup
    mockFullInitilizedConnector ();
    EXPECT_CALL(*mMessageCallbacks, onOrderAck(_, _)).Times(1);
    
    mockExecutionMessageRealTime ("0");
}

TEST_F(LseMillenniumTestHarness, TEST_THAT_ON_EXECUTION_EXEC_TYPE_FOUR_ON_ORDER_DONE_CALLBACK_IS_CALLED)
{
    // setup
    mockFullInitilizedConnector ();
    EXPECT_CALL(*mMessageCallbacks, onOrderDone(_, _)).Times(1);
    
    mockExecutionMessageRealTime ("4");
}

TEST_F(LseMillenniumTestHarness, TEST_THAT_ON_EXECUTION_EXEC_TYPE_FIVE_ON_MODIFY_ACK_CALLBACK_IS_CALLED)
{
    // setup
    mockFullInitilizedConnector ();
    EXPECT_CALL(*mMessageCallbacks, onModifyAck(_, _)).Times(1);
    
    mockExecutionMessageRealTime ("5");
}

TEST_F(LseMillenniumTestHarness, TEST_THAT_ON_EXECUTION_EXEC_TYPE_EIGHT_ON_ORDER_REJECTED_CALLBACK_IS_CALLED)
{
    // setup
    mockFullInitilizedConnector ();
    EXPECT_CALL(*mMessageCallbacks, onOrderRejected(_, _)).Times(1);
    
    mockExecutionMessageRealTime ("8");
}

TEST_F(LseMillenniumTestHarness, TEST_THAT_ON_EXECUTION_EXEC_TYPE_C_ON_ORDER_DONE_CALLBACK_IS_CALLED)
{
    // setup
    mockFullInitilizedConnector ();
    EXPECT_CALL(*mMessageCallbacks, onOrderDone(_, _)).Times(1);
    
    mockExecutionMessageRealTime ("C");
}

TEST_F(LseMillenniumTestHarness, TEST_THAT_ON_EXECUTION_EXEC_TYPE_F_ON_ORDER_FILL_CALLBACK_IS_CALLED)
{
    // setup
    mockFullInitilizedConnector ();
    EXPECT_CALL(*mMessageCallbacks, onOrderFill(_, _)).Times(1);
    
    mockExecutionMessageRealTime ("F");
}


TEST_F(LseMillenniumTestHarness, TEST_THAT_ON_EXECUTION_EXEC_TYPE_DEFAULT_ON_MSG_CALLBACK_IS_CALLED)
{
    // setup
    mockFullInitilizedConnector ();
    EXPECT_CALL(*mMessageCallbacks, onMsg(_, _)).Times(1);
    
    mockExecutionMessageRealTime ("E");
}
