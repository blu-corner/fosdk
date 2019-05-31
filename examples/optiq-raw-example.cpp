/** Testbed for gwcConnector **/

#include "gwcConnector.h"
#include "fields.h"
#include "optiqCodec.h"
#include "optiqConstants.h"
#include "optiqNewOrderPacket.h"

using namespace std;
using namespace neueda;

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
    }

    virtual void onRawMsg (uint64_t seqno, const void* data, size_t len)
    {
        // TODO: add raw handling
        mLog->info ("onRawMsg received - [seqnum: %lu] (%lu bytes) ...", seqno, len);

        cdr msg;
        size_t _;

        switch (mCodec.decode(msg, data, len, _))
        {
        case GW_CODEC_SUCCESS:
            mLog->info(msg.toString().c_str());
            break;

        case GW_CODEC_ERROR:
            mLog->err("unable to parse inbound msg");
            break;

        default:
            break;
        }
    }

    gwcConnector* mGwc;
    logger* mLog;
    optiqCodec mCodec;
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
    props.setProperty ("enable_raw_messages", "yes");

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

    struct timeval tv;
    gettimeofday (&tv, NULL);
    int64_t ts = tv.tv_sec * 1000000000 + tv.tv_usec * 1000;

    optiqNewOrderPacket msg;

    msg.setFirmID ("00099022");
    msg.setSendingTime (ts);
    msg.setClientOrderID (time (NULL));
    msg.setSymbolIndex (1110000);
    msg.setEMM (OPTIQ_EMM_CASH_AND_DERIVATIVE_CENTRAL_ORDER_BOOK);
    msg.setExecutionWithinFirmShortCode (3);
    msg.setTradingCapacity (OPTIQ_TRADINGCAPACITY_DEALING_ON_OWN_ACCOUNT);
    msg.setAccountType (OPTIQ_ACCOUNTTYPE_CLIENT);
    msg.setLPRole (OPTIQ_LPROLE_RETAIL_LIQUIDITY_PROVIDER);
    msg.setExecutionInstruction (OPTIQ_EXECUTIONINSTRUCTION_STPINCOMINGORDER);
    msg.setDarkExecutionInstruction (OPTIQ_DARKEXECUTIONINSTRUCTION_DARKINDICATOR);
    msg.setMiFIDIndicators (OPTIQ_MIFIDINDICATORS_EXECUTIONALGOINDICATOR);

    msg.setOrderPx (1234);
    msg.setOrderQty (500);
    msg.setTimeInForce (OPTIQ_TIMEINFORCE_DAY);
    msg.setOrderSide (OPTIQ_SIDE_BUY);
    msg.setOrderType (OPTIQ_ORDERTYPE_LIMIT);

    // Send a new order single
    if (!gwc->sendRaw (&msg, sizeof(optiqNewOrderPacket)))
    {
        errx (1, "failed to send new order...");
    }
    
    sleep (5);
    gwc->stop ();

    sleep (2);
    log->info ("destroying connector");
}
