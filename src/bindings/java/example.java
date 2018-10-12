import com.neueda.logger.*;
import com.neueda.config.*;
import com.neueda.fosdk.*;
import com.neueda.codec.*;
import com.neueda.cdr.*;


public class example
{
    public class LocalLogHandler extends LogHandler
    {
        public LocalLogHandler () { super(); }

        public LogSeverity getLevel() {
            return LogSeverity.DEBUG;
        }

        public String getFormat() {
            return "{severity} {name} {time} {message}";
        }

        public void handle (LogSeverity severity,
                            String name,
                            SWIGTYPE_p_tm tm_time,
                            SWIGTYPE_p_timeval tv,
                            String message,
                            long message_len)
        {
            String m = LogHandler.toString (getFormat (),
                                            severity,
                                            name,
                                            tm_time,
                                            tv,
                                            message,
                                            message_len);
            System.out.println (m);
        }
    }
    
    public class SessionCallbacks extends gwcSessionCallbacks
    {
        private gwcConnector gwc;
        private Logger logger;
        private String username;
        private String password;
        
        public SessionCallbacks(gwcConnector gwc, Logger logger, String username, String password)
        {
            super();
            this.gwc = gwc;
            this.logger = logger;
            this.username = username;
            this.password = password;
        }

        public void onConnected () {
            this.logger.log (LogSeverity.INFO, "session logged on...");
        }

        public void onLoggingOn (cdr msg) {
            this.logger.log (LogSeverity.INFO, "session logging on...");

            // set username and password
            msg.setString (Codecs.UserName, this.username);
            msg.setString (Codecs.Password, this.password);
        }

        public boolean onError (String errorMessage) {
            this.logger.log (LogSeverity.ERROR, "session err: " + errorMessage);

            // return true to attempt reconnection
            return true;
        }

        public void onLoggedOn (java.math.BigInteger seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "session logged on...");
        }

        public void onLoggedOff (java.math.BigInteger seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "session logged off...");
        }

        public void onGap (java.math.BigInteger expected, java.math.BigInteger recieved) {
            this.logger.log (LogSeverity.INFO, "gap deteched expected [" + expected + "] got [" + recieved + "]");
        }
    }

    public class MessageCallbacks extends gwcMessageCallbacks
    {
        private gwcConnector gwc;
        private Logger logger;
        
        public MessageCallbacks(gwcConnector gwc, Logger logger)
        {
            super();
            this.gwc = gwc;
            this.logger = logger;
        }
    
        public void onAdmin (java.math.BigInteger seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "onAdmin msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());
        }

        public void onOrderAck (java.math.BigInteger seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "onOrderAck msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());

            // get order id from ack
            String orderId = msg.getString (Codecs.OrderID);
        
            // send modify
            cdr modify = new cdr();
            modify.setString (Codecs.OriginalClientOrderID, "myorder");
            modify.setString (Codecs.ClientOrderID, "myorder1");
            modify.setString (Codecs.OrderID, orderId);
            modify.setInteger (Codecs.InstrumentID, 133215); // VOD.L
            modify.setInteger (Codecs.OrderQty, 2000);
            modify.setInteger (Codecs.OrderType, 2);
            modify.setDouble (Codecs.LimitPrice, 1234.56);
            modify.setInteger (Codecs.Side, 1);
            modify.setString (Codecs.Account, "account");
            modify.setInteger (Codecs.ExpireDateTime, 0);
            modify.setInteger (Codecs.DisplayQty, 0);
            modify.setDouble (Codecs.StopPrice, 0.0);
            modify.setInteger (Codecs.PassiveOnlyOrder, 0);
            modify.setInteger (Codecs.ClientID, 1234);
            modify.setInteger (Codecs.MinimumQuantity, 0);
            modify.setInteger (Codecs.PassiveOnlyOrder, 0);
            modify.setInteger (Codecs.ReservedField1, 0);
            modify.setInteger (Codecs.ReservedField2, 0);
            modify.setInteger (Codecs.ReservedField3, 0);
            modify.setInteger (Codecs.ReservedField4, 0);
    
            if (!this.gwc.sendModify (modify))
                this.logger.log (LogSeverity.ERROR, "failed to send modify");
        }

        public void onOrderRejected (java.math.BigInteger seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "onOrderRejected msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());
        }

        public void onOrderDone (java.math.BigInteger seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "onOrderDone msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());
        }

        public void onOrderFill (java.math.BigInteger seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "onOrderFill msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());
        }

        public void onModifyAck (java.math.BigInteger seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "onModifyAck msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());

            // cancel order
            String orderId = msg.getString (Codecs.OrderID);
        
            cdr cancel = new cdr ();
            cancel.setString (Codecs.OriginalClientOrderID, "myorder1");
            cancel.setString (Codecs.ClientOrderID, "myorder2");
            cancel.setString (Codecs.OrderID, orderId);
            cancel.setInteger (Codecs.InstrumentID, 133215); // VOD.L
            cancel.setInteger (Codecs.Side, 1);
            cancel.setString (Codecs.RfqID, "XXXX");

            cancel.setInteger (Codecs.ReservedField1, 0);
            cancel.setInteger (Codecs.ReservedField2, 0);
    
            if (!this.gwc.sendCancel (cancel)) {
                this.logger.log (LogSeverity.INFO, "failed to send cancel myorder1...");
            }
        }

        public void onModifyRejected (java.math.BigInteger seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "oModifyRejected msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());
        }

        public void onCancelRejected (java.math.BigInteger seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "onCacnelRejected msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());
        }

        public void onMsg (java.math.BigInteger seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "onMsg msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());
        }
    }

    public example() { }

    public void doMain()
    {
        LogService logService = LogService.get ();
        LocalLogHandler localLogHandler = new LocalLogHandler();
        try {
            logService.addHandler (localLogHandler);
        } catch (Exception e) {
            System.err.println("failed to add log handler");
            return;
        }   

        RawProperties properties = new RawProperties();
        Properties props = new Properties(properties, "gwc", "millennium", "sim");
        props.setProperty ("real_time_host", "127.0.0.1:9899");
        props.setProperty ("recovery_host", "127.0.0.1:10000");
        props.setProperty ("venue", "lse")

        Logger log = logService.getLogger ("MILLENIUM_TEST");
        gwcConnector gwc = gwcConnectorFactory.get (log, "millennium", props);
        if (gwc == null) {
            System.out.println ("failed to get connector...");
            return;
        }

        SessionCallbacks sessionCbs = new SessionCallbacks(gwc, log, "USER1", "PASS1");
        MessageCallbacks messageCbs = new MessageCallbacks(gwc, log);

        log.log (LogSeverity.INFO, "initialising connector...");
        if (!gwc.init (sessionCbs, messageCbs, props)) {
            System.out.println ("failed to initialise connector...");
            return;
        }

        log.log (LogSeverity.INFO, "starting connector...");
        if (!gwc.start (false)) {
            System.out.println ("failed to initialise connector...");
            return;
        }

        // wait for logon - blocks untill state change
        gwc.waitForLogon ();
 
        // send order
        gwcOrder order = new gwcOrder();
        order.setPrice (1234.45);
        order.setQty (java.math.BigInteger.valueOf(1000));
        order.setTif (gwcTif.GWC_TIF_DAY);
        order.setSide (gwcSide.GWC_SIDE_BUY);
        order.setOrderType (gwcOrderType.GWC_ORDER_TYPE_LIMIT);
        order.setString (Codecs.ClientOrderID, "myorder");
        order.setInteger (Codecs.InstrumentID, 133215); // VOD.L
        order.setInteger (Codecs.AutoCancel, 1);

        order.setString (Codecs.TraderID, "TX1");
        order.setString (Codecs.Account, "account");
        order.setInteger (Codecs.ClearingAccount, 1);
        order.setInteger (Codecs.FXMiFIDFlags, 0);
        order.setInteger (Codecs.PartyRoleQualifiers, 0);
        order.setInteger (Codecs.ExpireDateTime, 0);
        order.setInteger (Codecs.DisplayQty, 0);
        order.setInteger (Codecs.Capacity, 1);
        order.setInteger (Codecs.OrderSubType, 0);
        order.setInteger (Codecs.Anonymity, 0);
        order.setDouble (Codecs.StopPrice, 0.0);
        order.setInteger (Codecs.PassiveOnlyOrder, 0);
        order.setInteger (Codecs.ClientID, 1234);
        order.setInteger (Codecs.InvestmentDecisionMaker, 0);
        order.setInteger (Codecs.MinimumQuantity, 0);
        order.setInteger (Codecs.ExecutingTrader, 7676);

        log.log (LogSeverity.INFO, "sending-input-order");
        if (!gwc.sendOrder (order)) {
            System.out.println ("failed to send order myorder...");
            return;
        }

        try
        {
            Thread.sleep(5000);
        }
        catch(InterruptedException ex)
        {
            Thread.currentThread().interrupt();
        }

        gwc.stop ();
        
        try
        {
            Thread.sleep(2000);
        }
        catch(InterruptedException ex)
        {
            Thread.currentThread().interrupt();
        }

        
        log.log (LogSeverity.INFO, "destroying connector");
        logService.removeHandler (localLogHandler);
    }
    
    public static void main(String[] args)
    {
        example instance = new example();
        instance.doMain();
    }
}
