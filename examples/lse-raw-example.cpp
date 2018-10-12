/** Testbed for gwcConnector **/

#include "gwcConnector.h"
#include "fields.h"

#include "LsePackets.h"


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
        mLog->info ("session logging on...");

        /* set username and password */
        msg.setString (UserName, mUsername);
        msg.setString (Password, mPassword);
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
    string mUsername;
    string mPassword;
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

    virtual void onRawMsg (uint64_t seqno, const void* data, size_t len)
    {
        mLog->info ("onRawMsg...");

        const LseHeader* header = static_cast<const LseHeader*>(data);
        switch (header->mMessageType)
        {
        case '3':
            mLog->info ("received: reject");
            break;

        case '8':
            mLog->info ("received: execution report");
            break;

        default:
            mLog->err ("received: [%c]", header->mMessageType);
            break;
        }
        
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

    properties props (p, "gwc", "millennium", "sim");
    props.setProperty ("venue", "lse");
    props.setProperty ("real_time_host", "127.0.0.1:9899");
    props.setProperty ("recovery_host", "127.0.0.1:10000");
    props.setProperty ("enable_raw_messages", "true");

    logger* log = logService::getLogger ("MILLENIUM_TEST");

    sessionCallbacks sessionCbs;
    sessionCbs.mUsername = "USER1";
    sessionCbs.mPassword = "PASS1";

    sessionCbs.mLog = log;
    messageCallbacks messageCbs;
    messageCbs.mLog = log;

    gwcConnector* gwc = gwcConnectorFactory::get (log, "millennium", props);
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
 
    /* send order */
    LseNewOrder newOrder;
    newOrder.mHeader.mMessageType = 'D';
    newOrder.mHeader.mStartOfMessage = sizeof(LseHeader);
    newOrder.mHeader.mMessageLength = sizeof(LseNewOrder) - sizeof(LseHeader) - 1;
    newOrder.setLimitPrice(1234);
    newOrder.setOrderQty(1000);
    newOrder.setTIF(10);
    newOrder.setSide(1);
    newOrder.setOrderType(2);
    newOrder.setClientOrderID("myorder");
    newOrder.setInstrumentID(133215); // VOD.L
    newOrder.setAutoCancel(1);
    newOrder.setTraderID("TX1");
    newOrder.setAccount("account");
    newOrder.setClearingAccount(1);
    newOrder.setCapacity(1);
    newOrder.setClientID(1234);
    newOrder.setExecutingTrader(7676);

    if (!gwc->sendRaw (&newOrder, sizeof(newOrder)))
        errx  (1, "failed to send order myorder...");

    sleep (5);
    gwc->stop ();

    sleep (2);
    log->info ("destroying connector");

    return 0;
}
