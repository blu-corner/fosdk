#include "gwcConnector.h"
#include "fields.h"
#include "Timestamps.h"

using namespace std;
using namespace neueda;

static size_t kNumberOfOrders = 0;
static benchmark::TimestampFactory* kTsFactory = NULL;
static benchmark::TimestampFile* kOrderEntryTsFile = NULL;
static benchmark::TimestampFile* kOrderAckTsFile = NULL;


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
    neueda::Logger* mLog;
    string mUsername;
    string mPassword;
};

/* Message callbacks */
class messageCallbacks : public gwcMessageCallbacks
{
    
public:
    size_t mOffs;

    bool mDone;
    sbfCondVar mDoneCond;
    sbfMutex   mDoneMutex;
    
    messageCallbacks ()
        : gwcMessageCallbacks (),
          mOffs (0),
          mDone (false)
    {        
        sbfCondVar_init (&mDoneCond);
        sbfMutex_init (&mDoneMutex, 1);
    }

    void waitDone ()
    {
        while (mDone == false)
        {
            sbfMutex_lock (&mDoneMutex);
            sbfCondVar_wait (&mDoneCond, &mDoneMutex);
            sbfMutex_unlock (&mDoneMutex);
        }
    }
    
    virtual void onAdmin (uint64_t seqno, const cdr& msg)
    {
        mLog->info ("onAdmin msg...");
        mLog->info ("%s", msg.toString ().c_str ());
    }

    virtual void onOrderAck (uint64_t seqno, const cdr& msg)
    {
        // mLog->info ("onOrderAck msg...");
        // mLog->info ("%s", msg.toString ().c_str ());

        // log the timestamp
        kOrderAckTsFile->pushSample ();
        mOffs++;

        // is Done
        if (mOffs >= (kNumberOfOrders ))
        {
            mLog->info ("acked-all-orders-event");
            
            mDone = true;
            sbfMutex_lock (&mDoneMutex);
            sbfCondVar_signal (&mDoneCond);
            sbfMutex_unlock (&mDoneMutex);
        }
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
    neueda::Logger* mLog;
};

int doBenchmark(const char* config)
{
    std::string errorMessage;
    neueda::RawProperties properties;
    if (config == NULL)
    {
        errx (1, "config is empty");
    }
    bool ok = properties.loadFromFile(config, errorMessage);
    if (not ok)
    {
        std::string e("failed to load config: " + errorMessage);
        errx (1, "%s", e.c_str ());
    }

    ok = neueda::LogService::get ().configure (properties, errorMessage);
    if (not ok)
    {
        std::string e("failed to configure logger: " + errorMessage);
        errx (1, "%s", e.c_str ());
    }

    neueda::Properties props (properties, "gwc", "millennium", "sim");
    neueda::Properties benchmarkProperties (properties, "benchmark", "millennium", "lse");
    
    std::string numberMessagesStr;
    ok = benchmarkProperties.getProperty("number_messages", numberMessagesStr);
    if (not ok)
    {
        errx(1, "failed to get number of messages...");
    }
    kNumberOfOrders= std::atoi(numberMessagesStr.c_str());

    std::string timeStampMethod;
    ok = benchmarkProperties.getProperty("timestamp_method", timeStampMethod);
    if (not ok)
    {
        errx(1, "failed to get timestamp method...");
    }
    
    bool isGTOD = timeStampMethod.compare("gettimeofday");
    bool isRDTSC = timeStampMethod.compare("cpu perf counter");
    
    if (isGTOD) 
    {
        kTsFactory = new benchmark::TimestampFactory(benchmark::GTOD);
    }
    else if (isRDTSC)
    {
        kTsFactory = new benchmark::TimestampFactory(benchmark::RDTSC);
    }
    else
    {
        errx(1, "unknown timestamp method %s", timeStampMethod.c_str ());
    }
    
    kOrderEntryTsFile = new benchmark::TimestampFile(kNumberOfOrders, *kTsFactory);
    kOrderAckTsFile = new benchmark::TimestampFile(kNumberOfOrders, *kTsFactory);

    neueda::Logger* log = neueda::LogService::get ().getLogger ("MILLENIUM_TEST");

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

    for (size_t i = 0; i < kNumberOfOrders; ++i)
    {
        // log the timestamp
        kOrderEntryTsFile->pushSample ();
        
        std::ostringstream oss;
        oss << "myOrderId"
            << (i + 1);
        
        std::string clientOrderId = oss.str ();
        
        gwcOrder order;
        order.setPrice (1234.45);
        order.setQty (1000);
        order.setTif (GWC_TIF_DAY);
        order.setSide (GWC_SIDE_BUY);
        order.setOrderType (GWC_ORDER_TYPE_LIMIT);
        order.setString (ClientOrderID, clientOrderId);
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
        
        if (!gwc->sendOrder (order))
        {
            log->err ("failed to send order myorder...");
            log->err ("stopping due to failed order entry");
            break;
        }
    }

    log->info ("done sending orders");

    // wait until done
    messageCbs.waitDone ();
    
    log->info ("destroying connector");
    gwc->stop ();
    delete gwc;

    log->info ("writing time stamps on order entry call");
    kOrderEntryTsFile -> saveToDisk ("order-entry-called-timestamps.dat");

    log->info ("writing time stamps on order acked");
    kOrderAckTsFile -> saveToDisk ("order-acked-timestamps.dat");

    log->info ("done");

    delete kOrderAckTsFile;
    delete kOrderEntryTsFile;
    delete kTsFactory;

    return 0;
}

int main (int argc, char** argv)
{
    char *config = NULL;
    int c;

    while((c = getopt (argc, argv, "c:")) != -1)
        switch (c)
        {
            case 'c':
                config = optarg;
                break;
            
            case '?':
                if (optopt == 'c')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
            return 1;
            
            default:
                abort ();
        };
                
    doBenchmark(config); 
}
