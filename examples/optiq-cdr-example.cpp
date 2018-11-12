/** Testbed for gwcConnector **/

#include "gwcConnector.h"
#include "fields.h"
#include "optiqConstants.h"

using namespace std;
using namespace neueda;

static int64_t gOrderId;

class sessionCallbacks : public gwcSessionCallbacks
{
public:
    virtual void onConnected ()
    {
        mLog->info ("session logged on...");
    }

    virtual void onLoggingOn (cdr& msg)
    {
        //static bool response = false;
        mLog->info ("session logging on...");
        msg.setString (SoftwareProvider, "BLUCNR");
        msg.setInteger (QueueingIndicator, 1);    
    }

    virtual bool onError (const string& err)
    {
        mLog->err ("session err [%s]", err.c_str ());
        return true; /* try to reconnect */
    }

    virtual void onLoggedOn (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("session logged on...");

    }

    virtual void onTraderLogonOn (std::string traderId, const cdr& msg)
    {
        mLog->info ("trader logged on...");
    }

    virtual void onLoggedOff (uint64_t seqno, const cdr& msg) 
    {
        mLog->info ("session logged off...");
    }

    virtual void onGap (uint64_t expected, uint64_t recieved)
    {
        mLog->warn ("gap detected expected [%llu] got [%llu] ",
                   (unsigned long long)expected,   
                   (unsigned long long)recieved);
    };

    gwcConnector* mGwc;
    logger* mLog;
};

/* Message callbacks */
class messageCallbacks : public gwcMessageCallbacks
{
public:
    virtual void onAdmin (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onAdmin msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    virtual void onOrderAck (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onOrderAck msg...");
        mLog->info ("%s", msg.toString ().c_str ());
        msg.getInteger (OrderID, gOrderId);
    }

    virtual void onOrderRejected (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onOrderRejected msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    virtual void onOrderDone (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onOrderDone msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    virtual void onOrderFill (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onOrderFill msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    virtual void onModifyAck (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onModifyAck msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    virtual void onModifyRejected (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("oModifyRejected msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    virtual void onCancelRejected (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onCacnelRejected msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    virtual void onMsg (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onMsg msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    gwcConnector* mGwc;
    logger* mLog;
};

int main (int argc, char** argv)
{
    properties p;
    p.setProperty ("lh.console.level", "debug");
    p.setProperty ("lh.console.color", "true");

    std::string errorMessage;
    bool ok = logService::get ().configure (p, errorMessage);
    if (not ok)
    {
        std::string e("failed to configure logger: " + errorMessage);
        errx (1, "%s", e.c_str ());
    }

    properties props (p, "gwc", "optiq", "sim");
    props.setProperty ("host", "127.0.0.1:9999");
    props.setProperty ("partition", "10");
    props.setProperty ("accessId", "1024");

    logger* log = logService::getLogger ("OPTIQ_TEST");

    sessionCallbacks sessionCbs;

    sessionCbs.mLog = log;
    messageCallbacks messageCbs;
    messageCbs.mLog = log;

    gwcConnector* gwc = gwcConnectorFactory::get (log, "optiq", props);

    if (gwc == NULL)
        errx (1, "failed to get connector...");

    sessionCbs.mGwc = gwc;
    messageCbs.mGwc = gwc;

    log->info ("initialising connector...");
    if (!gwc->init (&sessionCbs, &messageCbs, props))
        errx (1, "failed to initialise connector...");

    log->info ("starting connector...");
    if (!gwc->start (false))
        errx (1, "failed to initialise connector...");
  
    gwc->waitForLogon ();

    

    /* send sell order */
    struct timeval tv;
    gettimeofday (&tv, NULL);
    int64_t ts = tv.tv_sec * 1000000000 + tv.tv_usec * 1000;
    gwcOrder border;
    border.setPrice (1234);
    border.setQty (500);
    border.setTif (GWC_TIF_DAY);
    border.setSide (GWC_SIDE_BUY);
    border.setOrderType (GWC_ORDER_TYPE_LIMIT);

    border.setString (FirmID, "00099022");
    border.setInteger (SendingTime, ts);
    int64_t clOrdId = time (NULL);
    border.setInteger (ClientOrderID, clOrdId);
    border.setInteger (SymbolIndex, 1110000);
    border.setInteger (EMM, OPTIQ_EMM_CASH_AND_DERIVATIVE_CENTRAL_ORDER_BOOK);
    border.setInteger (ExecutionWithinFirmShortCode, 3);
    border.setInteger (TradingCapacity, 
                       OPTIQ_TRADINGCAPACITY_DEALING_ON_OWN_ACCOUNT);
    border.setInteger (AccountType, OPTIQ_ACCOUNTTYPE_CLIENT);
    border.setInteger (LPRole, OPTIQ_LPROLE_RETAIL_LIQUIDITY_PROVIDER);
    border.setInteger (ExecutionInstruction, 
                       OPTIQ_EXECUTIONINSTRUCTION_STPRESTINGORDER);
    border.setInteger (DarkExecutionInstruction, 
                       OPTIQ_DARKEXECUTIONINSTRUCTION_DARKINDICATOR);
    border.setInteger (MiFIDIndicators, 
                       OPTIQ_MIFIDINDICATORS_EXECUTIONALGOINDICATOR);

    if (!gwc->sendOrder (border))
        errx  (1, "failed to send border myborder...");
    sleep (5);

    /* send sell order */
    gettimeofday (&tv, NULL);
    ts = tv.tv_sec * 1000000000 + tv.tv_usec * 1000;
    gwcOrder sorder;
    sorder.setPrice (1234);
    sorder.setQty (1000);
    sorder.setTif (GWC_TIF_DAY);
    sorder.setSide (GWC_SIDE_SELL);
    sorder.setOrderType (GWC_ORDER_TYPE_MARKET);

    sorder.setString (FirmID, "00099022");
    sorder.setInteger (SendingTime, ts);
    clOrdId = time (NULL);
    sorder.setInteger (ClientOrderID, clOrdId);
    sorder.setInteger (SymbolIndex, 1110000);
    sorder.setInteger (EMM, OPTIQ_EMM_CASH_AND_DERIVATIVE_CENTRAL_ORDER_BOOK);
    sorder.setInteger (ExecutionWithinFirmShortCode, 3);
    sorder.setInteger (TradingCapacity, 
                       OPTIQ_TRADINGCAPACITY_DEALING_ON_OWN_ACCOUNT);
    sorder.setInteger (AccountType, OPTIQ_ACCOUNTTYPE_CLIENT);
    sorder.setInteger (LPRole, OPTIQ_LPROLE_RETAIL_LIQUIDITY_PROVIDER);
    sorder.setInteger (ExecutionInstruction, 
                       OPTIQ_EXECUTIONINSTRUCTION_STPRESTINGORDER);
    sorder.setInteger (DarkExecutionInstruction, 
                       OPTIQ_DARKEXECUTIONINSTRUCTION_DARKINDICATOR);
    sorder.setInteger (MiFIDIndicators, 
                       OPTIQ_MIFIDINDICATORS_EXECUTIONALGOINDICATOR);

    if (!gwc->sendOrder (sorder))
        errx  (1, "failed to send sorder myborder...");
    sleep (5);

#if 0
    /* send a cancel */
    cdr cancel;
    cancel.setString (FirmID, "00099022");
    cancel.setInteger (SendingTime, ts);
    cancel.setInteger (ExecutionWithinFirmShortCode, 0);
    cancel.setInteger (ClientOrderID, -1161014899999999951LL);
    cancel.setInteger (OrigClientOrderID, clOrdId);
    cancel.setInteger (OrderID, gOrderId);
    cancel.setInteger (SymbolIndex, 1110000);
    cancel.setInteger (EMM, OPTIQ_EMM_Cash_and_Derivative_Central_Order_Book);
    cancel.setInteger (OrderSide, OPTIQ_OrderSide_Buy);
    cancel.setInteger (OrderType, OPTIQ_OrderType_Limit);

    if (!gwc->sendCancel (cancel))
        errx  (1, "failed to send cancel...");
    sleep (5);
#endif

#if 0
    /* send a modify */
    cdr modify;
    modify.setInteger (OrderPx, 1234);
    modify.setInteger (OrderQty, 1000);
    modify.setInteger (TimeInForce, 0);
    modify.setInteger (OrderSide, 2);
    modify.setInteger (OrderType, 2);

    modify.setString (FirmID, "00099022");
    modify.setInteger (SendingTime, ts);
    //modify.setInteger (OrigClientOrderID, clOrdId);
    //modify.setInteger (ClientOrderID, time (NULL));
    modify.setInteger (OrigClientOrderID, clOrdId);
    modify.setInteger (ClientOrderID, -1161014899999999951LL);
    modify.setInteger (SymbolIndex, 1110000);
    modify.setInteger (EMM, OPTIQ_EMM_Cash_and_Derivative_Central_Order_Book);
    modify.setInteger (ExecutionWithinFirmShortCode, 3);
    modify.setInteger (TradingCapacity, 
                       OPTIQ_TradingCapacity_Dealing_on_own_account);
    modify.setInteger (AccountType, OPTIQ_AccountType_Client);
    modify.setInteger (LPRole, OPTIQ_LPRole_Retail_Liquidity_Provider);
    modify.setInteger (ExecutionInstruction, 
                       OPTIQ_ExecutionInstruction_STPRestingOrder);
    modify.setInteger (DarkExecutionInstruction, 
                       OPTIQ_DarkExecutionInstruction_DarkIndicator);
    modify.setInteger (MiFIDIndicators, 
                       OPTIQ_MiFIDIndicators_ExecutionAlgoIndicator);

    if (!gwc->sendModify (modify))
        errx  (1, "failed to send modify myborder...");
    sleep (5);
#endif

    sleep (5);
    gwc->stop ();

    sleep (2);
    log->info ("destroying connector");

    return 0;
}
