#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "gwcEti.h"
#include "TestUtils.h"

using namespace neueda;
using namespace ::testing;


class MockXetraConnector : public gwcEti<xetraCodec>
{
public:
    MockXetraConnector (logger* log)
        : gwcEti<xetraCodec> (log)
    { }

    MOCK_METHOD1 (start, bool(bool reset));
    
    MOCK_METHOD0 (reset, void());

    void setTcpConnection (SbfTcpConnection** connection)
    {
        if (mTcpConnection)
            delete mTcpConnection;
        mTcpConnection = *connection;
    }

    void mockTcpConnectionReady ()
    {
        mTcpConnectionDelegate.onReady ();
    }

    void mockTcpConnectionError ()
    {
        mTcpConnectionDelegate.onError ();
    }

    size_t mockTcpConnectionRead (void* data, size_t len)
    {
        return mTcpConnectionDelegate.onRead (data, len);
    }
};

class XetraEtiTestHarness : public Test
{
protected:
    virtual void SetUp()
    {
        ::remove ("xetra.seqno.cache");
        
        mLogger = logService::getLogger ("TEST_XETRA");
        mProps = new properties("gwc", "xetra", "sim");

        mSessionCallbacks = new MockSessionCallbacks();
        mMessageCallbacks = new MockMessageCallbacks();

        mConnector = new MockXetraConnector(mLogger);
        mMockConnectionActive = false;
    }

    virtual void TearDown()
    {
        EXPECT_CALL (*mSessionCallbacks, onLoggedOff(_, _))
            .Times(1);
        mConnector->stop();

        if (mMockConnectionActive)
        {
            EXPECT_CALL(*mMessageCallbacks, onAdmin(_, _))
                .Times (1);
            mockLogoffReply ();
        }

        delete mConnector;
        delete mSessionCallbacks;
        delete mMessageCallbacks;
        delete mProps;
    }

    cdr setLogonReplyMessage (cdr d)
    {
        d.setInteger (TemplateID, 10001);
        d.setInteger (MsgSeqNum, 2);
        d.setString (DefaultCstmApplVerID, "22222");

        return d;
    }

    cdr setReject (cdr d, int rejectReason, int status)
    {
        d.setInteger (TemplateID, 10010);
        d.setInteger (MsgSeqNum, 2);
        d.setString (DefaultCstmApplVerID, "22222");
        d.setInteger (SessionStatus, status);
        d.setInteger (SessionRejectReason, rejectReason);

        return d;
    }

    cdr setLogoutMessage (cdr d, string reason)
    {
        d.setInteger (TemplateID, 10012);
        d.setInteger (MsgSeqNum, 1);

        return d;
    }

    cdr setCdr (cdr d, int templateID, string status)
    {
        d.setInteger (MsgSeqNum, 10);
        d.setInteger (TemplateID, templateID);
        d.setInteger (SendingTime, 1558520998956394194);
        d.setInteger (PartitionID, 31);
        d.setString (ApplMsgID, "mockID");
        d.setInteger (ApplID, 4);
        d.setInteger (ApplResendFlag, 0);
        d.setInteger (LastFragment, 1);
        d.setInteger (OrderID, 12345);
        d.setInteger (ClOrdID, 12345);
        d.setInteger (SecurityID, 1010);
        d.setInteger (ExecID, 1558520998956394194);
        d.setInteger (TrdRegTSEntryTime, 1558520998956394194);
        d.setInteger (TrdRegTSTimePriority, 1558520998956394194);
        d.setInteger (OrderIDSfx, 1);
        d.setString (OrdStatus, status);
        d.setString (ExecType, "0");
        d.setInteger (ExecRestatementReason, 101);
        d.setInteger (CrossedIndicator, 0);
        d.setInteger (Triggered, 0);
        d.setInteger (TransactionDelayIndicator, 0);

        switch (templateID)
        {
        case 10102:
            d.setString (ExecType, "0");
            break;
        case 10103:
        case 10104:
            if (templateID == 10103)
                d.setInteger (TrdRegTSTimeIn, 1558520998956394194);
            d.setInteger (TrdRegTSTimeOut, 1558520998956394194);
            d.setDouble (LeavesQty, 0);
            d.setDouble (CumQty, 10);
            d.setDouble (CxlQty, 0);
            d.setInteger (OrigClOrdID, 12345);
            d.setInteger (MarketSegmentID, 12345);
            d.setInteger (Side, 1);
        case 10108:
            d.setString (ExecType, "5");
            d.setDouble (LeavesQty, 10);
            d.setDouble (CumQty, 0);
            d.setInteger (OrigClOrdID, 12345);
            break;
        default:
            break;
        }

        return d;
    }

    void mockInitilizeConnector ()
    {
        mProps->setProperty ("host", "127.0.0.1:9899");
        mProps->setProperty ("partition", "31");
        mProps->setProperty ("venue", "xetra");
        bool ok = mConnector->init (mSessionCallbacks,
                                    mMessageCallbacks,
                                    *mProps);
        ASSERT_TRUE(ok);

        setupMockTcpConnection ();
    }

    void setupMockTcpConnection ()
    {
        mTcpDelegate =
                new gwcEtiTcpConnectionDelegate<xetraCodec>(mConnector);
        mMockTcpConnection = new MockSbfTcpConnection (
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            true,
            mTcpDelegate);
        mConnector->setTcpConnection ((SbfTcpConnection**)&mMockTcpConnection);

        EXPECT_CALL (*mMockTcpConnection, send (_, _))
            .Times (AnyNumber ());
        EXPECT_CALL (*mMockTcpConnection, connect ())
            .Times (AnyNumber ())
            .WillRepeatedly(Return(true));
    }

    void mockTcpMessage (cdr& msg)
    {
        xetraCodec codec;

        char space[1024];
        size_t used;
        codecState state = codec.encode (msg, space, sizeof space, used);
        
        if (state != GW_CODEC_SUCCESS)
            std::cout << codec.getLastError () << std::endl;

        ASSERT_EQ(state, GW_CODEC_SUCCESS);
        
        mTcpDelegate->onRead (space, used);
    }

    void mockLogonReply ()
    {
        cdr d;
        d = setLogonReplyMessage (d);
        
        mockTcpMessage (d);
    }

    void mockLogonReject ()
    {
        cdr d;
        d = setReject (d, 99, 4);

        mockTcpMessage (d);
    }

    void mockLogoffReply ()
    {
        cdr d;
        d = setLogoutMessage (d, "mock");
        
        mockTcpMessage (d);
    }

    void mockRejectMessage ()
    {
        cdr d;
        d = setReject (d, 99, 0);

        mockTcpMessage (d);
    }

    void mockOrderBookExecution (string status)
    {
        cdr d;
        d = setCdr (d, 10104, status);

        mockTcpMessage (d);
    }

    void mockImmediateExecution (string status)
    {
        cdr d;
        d = setCdr (d, 10103, status);

        mockTcpMessage (d);
    }

    void mockOrderAck (string status)
    {
        cdr d;
        d = setCdr (d, 10102, status);

        mockTcpMessage (d);
    }

    void mockModifyAck (string status)
    {
        cdr d;
        d = setCdr (d, 10108, status);

        mockTcpMessage (d);
    }

    void mockFullInitilizedConnector ()
    {
        mockInitilizeConnector ();
        EXPECT_CALL(*mSessionCallbacks, onLoggingOn(_))
            .Times(1);
        
        mConnector->mockTcpConnectionReady ();        
        EXPECT_CALL(*mMessageCallbacks, onAdmin(_, _))
            .Times (1);
        EXPECT_CALL(*mSessionCallbacks, onLoggedOn(_, _))
            .Times(1);
        mockLogonReply ();

        mMockConnectionActive = true;
    }

    gwcOrder getMockNewOrder ()
    {
        gwcOrder order;
        
        int64_t clOrdId (time(0));
        order.setPrice (1234.45);
        order.setQty (5);
        order.setTif (GWC_TIF_DAY);
        order.setSide (GWC_SIDE_BUY);
        order.setOrderType (GWC_ORDER_TYPE_LIMIT);
        order.setInteger (SenderSubID, 123456789);
        order.setInteger (ClOrdID, clOrdId);
        order.setInteger (SecurityID, 485241),
        order.setInteger (PartyIDClientID, 0);
        order.setInteger (PartyIdInvestmentDecisionMaker, 2000000003);
        order.setInteger (ExecutingTrader, 2000005140);
        order.setInteger (MarketSegmentID, 20260);
        order.setInteger (ApplSeqIndicator, 0);
        order.setInteger (PriceValidityCheckType, 0);    
        order.setInteger (ValueCheckTypeValue, 0);
        order.setInteger (ValueCheckTypeQuantity, 0);
        order.setInteger (OrderAttributeLiquidityProvision, 0);
        order.setInteger (ExecInst, 2);
        order.setInteger (TradingCapacity, 5);
        order.setInteger (PartyIdInvestmentDecisionMakerQualifier, 24);
        order.setInteger (ExecutingTraderQualifier, 24);

        return order;
    }

    logger* mLogger;
    properties* mProps;
    MockSessionCallbacks* mSessionCallbacks; 
    MockMessageCallbacks* mMessageCallbacks;
    MockXetraConnector* mConnector;

    MockSbfTcpConnection* mMockTcpConnection;
    SbfTcpConnectionDelegate* mTcpDelegate;
    
    bool mMockConnectionActive;
};

// TESTS

TEST_F(XetraEtiTestHarness, TEST_THAT_INIT_FAILS_ON_MISSING_HOST_PARAM)
{
    mProps->setProperty ("partition", "31");
    mProps->setProperty ("venue", "xetra");
    bool ok = mConnector->init(mSessionCallbacks, mMessageCallbacks, *mProps);
    ASSERT_FALSE(ok);

}
TEST_F(XetraEtiTestHarness, TEST_THAT_INIT_FAILS_ON_INVALID_HOST_PARAM)
{
    mProps->setProperty ("host", "Test.");
    mProps->setProperty ("partition", "31");
    mProps->setProperty ("venue", "xetra");
    bool ok = mConnector->init(mSessionCallbacks, mMessageCallbacks, *mProps);
    ASSERT_FALSE(ok);
}

TEST_F(XetraEtiTestHarness, TEST_THAT_INIT_SUCCEEDS_ON_VALID_PARAMS)
{
    mProps->setProperty ("host", "127.0.0.1:9899");
    mProps->setProperty ("partition", "31");
    mProps->setProperty ("venue", "xetra");
    bool ok = mConnector->init(mSessionCallbacks, mMessageCallbacks, *mProps);
    ASSERT_TRUE(ok);

    // helps give chance for sbf_queue to cleanup properly
    sleep(1);
}

TEST_F(XetraEtiTestHarness, TEST_THAT_ON_CONNECTION_READY_ONLOGGINGON_IS_CALLED)
{
    // setup
    mockInitilizeConnector ();
    
    // test
    EXPECT_CALL(*mSessionCallbacks, onLoggingOn(_)).Times(1);
    mConnector->mockTcpConnectionReady ();
}

TEST_F(XetraEtiTestHarness, TEST_THAT_ON_CONNECTION_READY_WITH_LOGON_REPLY_ON_ADMIN_IS_CALLED)
{
    // setup
    mockInitilizeConnector ();

    // test
    EXPECT_CALL(*mSessionCallbacks, onLoggingOn(_)).Times(1);
    mConnector->mockTcpConnectionReady ();        

    EXPECT_CALL(*mMessageCallbacks, onAdmin(_, _))
        .Times (1);
    EXPECT_CALL(*mSessionCallbacks, onLoggedOn(_, _)).Times(1);
    mockLogonReply ();
    mMockConnectionActive = true;
}

TEST_F(XetraEtiTestHarness, TEST_THAT_ON_CONNECTION_READY_WITH_LOGON_REJECT_ON_ERROR_IS_CALLED)
{
    // setup
    mockInitilizeConnector ();

    // test
    EXPECT_CALL(*mSessionCallbacks, onLoggingOn(_)).Times(1);
    mConnector->mockTcpConnectionReady ();
    
    EXPECT_CALL(*mMessageCallbacks, onMsg(_, _))
        .Times(1);
    EXPECT_CALL(*mSessionCallbacks, onError(_))
        .Times(1);
    mockLogonReject ();
}

TEST_F(XetraEtiTestHarness, TEST_THAT_CANNOT_SEND_ORDER_IF_NOT_LOGGED_ON)
{
    // setup
    mockInitilizeConnector ();
    EXPECT_CALL(*mSessionCallbacks, onLoggingOn(_)).Times(1);
    
    mConnector->mockTcpConnectionReady ();

    // do test
    gwcOrder mockOrder = getMockNewOrder ();
    bool ok = mConnector->sendOrder (mockOrder);

    // check
    ASSERT_FALSE (ok);
}

TEST_F(XetraEtiTestHarness, TEST_THAT_CAN_SEND_ORDER_IF_LOGGED_ON)
{
    // setup
    mockFullInitilizedConnector ();
    
    // do test
    gwcOrder mockOrder = getMockNewOrder ();
    bool ok = mConnector->sendOrder (mockOrder);

    // check
    ASSERT_TRUE (ok);
}

TEST_F(XetraEtiTestHarness, TEST_THAT_ON_ORDER_ACK_CALLBACK_IS_CALLED)
{
    // setup
    mockFullInitilizedConnector ();

    // test
    EXPECT_CALL(*mMessageCallbacks, onOrderAck(_, _)).Times(1);
    mockOrderAck ("0");
}

TEST_F(XetraEtiTestHarness, TEST_THAT_ON_ORDER_DONE_CALLBACK_IS_CALLED_WITH_ORD_STATUS_FOUR_IN_ORDER_ACK)
{
    // setup
    mockFullInitilizedConnector ();

    // test
    EXPECT_CALL(*mMessageCallbacks, onOrderAck(_, _)).Times(1);
    EXPECT_CALL(*mMessageCallbacks, onOrderDone(_, _)).Times(1);
    mockOrderAck ("4");
}

TEST_F(XetraEtiTestHarness, TEST_THAT_ON_MODIFY_ACK_CALLBACK_IS_CALLED)
{
    // setup
    mockFullInitilizedConnector ();

    // test
    EXPECT_CALL(*mMessageCallbacks, onModifyAck(_, _)).Times(1);
    mockModifyAck ("0");
}

TEST_F(XetraEtiTestHarness, TEST_THAT_ON_ORDER_DONE_CALLBACK_IS_CALLED_WITH_ORD_STATUS_FOUR_IN_MODIFY_ACK)
{
    // setup
    mockFullInitilizedConnector ();

    // test
    EXPECT_CALL(*mMessageCallbacks, onModifyAck(_, _)).Times(1);
    EXPECT_CALL(*mMessageCallbacks, onOrderDone(_, _)).Times(1);
    mockModifyAck ("4");
}

TEST_F(XetraEtiTestHarness, TEST_THAT_ON_ORDER_REJECTED_CALLBACK_IS_CALLED)
{
    // setup
    mockFullInitilizedConnector ();

    // test
    EXPECT_CALL(*mMessageCallbacks, onOrderRejected(_, _)).Times(1);
    mockRejectMessage ();
}

TEST_F(XetraEtiTestHarness, TEST_THAT_ON_IMMEDIATE_FILL_ORDER_ACK_AND_ORDER_FILL_CALLBACKS_ARE_CALLED)
{
    // setup
    mockFullInitilizedConnector ();
    
    // test
    EXPECT_CALL(*mMessageCallbacks, onOrderAck(_, _)).Times(1);
    EXPECT_CALL(*mMessageCallbacks, onOrderFill(_, _)).Times(1);
    mockImmediateExecution ("2");
}

TEST_F(XetraEtiTestHarness, TEST_THAT_ON_ORDER_BOOK_EXECUTION_ON_ORDER_FILL_CALLBACK_IS_CALLED)
{
    // setup
    mockFullInitilizedConnector ();
    
    // test
    EXPECT_CALL(*mMessageCallbacks, onOrderFill(_, _)).Times(1);
    mockOrderBookExecution ("2");
}
