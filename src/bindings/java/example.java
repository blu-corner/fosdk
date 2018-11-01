import com.neueda.logger.*;
import com.neueda.properties.*;
import com.neueda.fosdk.*;
import com.neueda.codec.*;
import com.neueda.cdr.*;


public class example
{
    public class LocalLogHandler extends LogHandler
    {
        public LocalLogHandler () { super(); }

        public LogSeverity.level getLevel() {
            return LogSeverity.level.DEBUG;
        }

        public String getFormat() {
            return "{severity} {name} {time} {message}";
        }

        public void handle (LogSeverity.level severity,
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
            this.logger.info("session logged on...");
        }

        public void onLoggingOn (Cdr msg) {
            this.logger.info("session logging on...");

            // set username and password
            msg.setString (Fields.UserName, this.username);
            msg.setString (Fields.Password, this.password);
        }

        public boolean onError (String errorMessage) {
            this.logger.err( "session err: " + errorMessage);

            // return true to attempt reconnection
            return true;
        }

        public void onLoggedOn (java.math.BigInteger seqno, Cdr msg) {
            this.logger.info("session logged on...");
        }

        public void onLoggedOff (java.math.BigInteger seqno, Cdr msg) {
            this.logger.info("session logged off...");
        }

        public void onGap (java.math.BigInteger expected, java.math.BigInteger recieved) {
            this.logger.info("gap deteched expected [" + expected + "] got [" + recieved + "]");
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
    
        public void onAdmin (java.math.BigInteger seqno, Cdr msg) {
            this.logger.info("onAdmin msg...");
            this.logger.info(msg.toString ());
        }

        public void onOrderAck (java.math.BigInteger seqno, Cdr msg) {
            this.logger.info("onOrderAck msg...");
            this.logger.info(msg.toString ());

            // get order id from ack
            // get order id from ack
            String orderId = msg.getString (Fields.OrderID);
        
            // send modify
            Cdr modify = new Cdr();
            modify.setString (Fields.OriginalClientOrderID, "myorder");
            modify.setString (Fields.ClientOrderID, "myorder1");
            modify.setString (Fields.OrderID, orderId);
            modify.setInteger (Fields.InstrumentID, 133215); // VOD.L
            modify.setInteger (Fields.OrderQty, 2000);
            modify.setInteger (Fields.OrderType, 2);
            modify.setDouble (Fields.LimitPrice, 1234.56);
            modify.setInteger (Fields.Side, 1);
            modify.setString (Fields.Account, "account");
            modify.setInteger (Fields.ExpireDateTime, 0);
            modify.setInteger (Fields.DisplayQty, 0);
            modify.setDouble (Fields.StopPrice, 0.0);
            modify.setInteger (Fields.PassiveOnlyOrder, 0);
            modify.setInteger (Fields.ClientID, 1234);
            modify.setInteger (Fields.MinimumQuantity, 0);
            modify.setInteger (Fields.PassiveOnlyOrder, 0);
            modify.setInteger (Fields.ReservedField1, 0);
            modify.setInteger (Fields.ReservedField2, 0);
            modify.setInteger (Fields.ReservedField3, 0);
            modify.setInteger (Fields.ReservedField4, 0);
    
            if (!this.gwc.sendModify (modify))
                this.logger.err("failed to send modify");
        }

        public void onOrderRejected (java.math.BigInteger seqno, Cdr msg) {
            this.logger.info("onOrderRejected msg...");
            this.logger.info(msg.toString ());
        }

        public void onOrderDone (java.math.BigInteger seqno, Cdr msg) {
            this.logger.info("onOrderDone msg...");
            this.logger.info(msg.toString ());
        }

        public void onOrderFill (java.math.BigInteger seqno, Cdr msg) {
            this.logger.info("onOrderFill msg...");
            this.logger.info(msg.toString ());
        }

        public void onModifyAck (java.math.BigInteger seqno, Cdr msg) {
            this.logger.info("onModifyAck msg...");
            this.logger.info(msg.toString ());

            // cancel order
            String orderId = msg.getString (Fields.OrderID);
        
            Cdr cancel = new Cdr ();
            cancel.setString (Fields.OriginalClientOrderID, "myorder1");
            cancel.setString (Fields.ClientOrderID, "myorder2");
            cancel.setString (Fields.OrderID, orderId);
            cancel.setInteger (Fields.InstrumentID, 133215); // VOD.L
            cancel.setInteger (Fields.Side, 1);
            cancel.setString (Fields.RfqID, "XXXX");

            cancel.setInteger (Fields.ReservedField1, 0);
            cancel.setInteger (Fields.ReservedField2, 0);
    
            if (!this.gwc.sendCancel (cancel)) {
                this.logger.info( "failed to send cancel myorder1...");
            }
        }

        public void onModifyRejected (java.math.BigInteger seqno, Cdr msg) {
            this.logger.info("oModifyRejected msg...");
            this.logger.info(msg.toString ());
        }

        public void onCancelRejected (java.math.BigInteger seqno, Cdr msg) {
            this.logger.info("onCacnelRejected msg...");
            this.logger.info(msg.toString ());
        }

        public void onMsg (java.math.BigInteger seqno, Cdr msg) {
            this.logger.info("onMsg msg...");
            this.logger.info(msg.toString ());
        }
    }

    public example() { }

    public void doMain()
    {
        LogService logService = LogService.get ();
                                                                           
        Properties props = new Properties("gwc", "millennium", "sim");
        props.setProperty ("real_time_host", "127.0.0.1:9899");
        props.setProperty ("recovery_host", "127.0.0.1:10000");
        props.setProperty ("venue", "lse");

        Logger log = logService.getLogger ("MILLENIUM_TEST");
        gwcConnector gwc = gwcConnectorFactory.get (log, "millennium", props);
        if (gwc == null) {
            System.out.println ("failed to get connector...");
            return;
        }

        SessionCallbacks sessionCbs = new SessionCallbacks(gwc, log, "USER1", "PASS1");
        MessageCallbacks messageCbs = new MessageCallbacks(gwc, log);

        log.info("initialising connector...");
        if (!gwc.init (sessionCbs, messageCbs, props)) {
            System.out.println ("failed to initialise connector...");
            return;
        }

        log.info("starting connector...");
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
        order.setString (Fields.ClientOrderID, "myorder");
        order.setInteger (Fields.InstrumentID, 133215); // VOD.L
        order.setInteger (Fields.AutoCancel, 1);

        order.setString (Fields.TraderID, "TX1");
        order.setString (Fields.Account, "account");
        order.setInteger (Fields.ClearingAccount, 1);
        order.setInteger (Fields.FXMiFIDFlags, 0);
        order.setInteger (Fields.PartyRoleQualifiers, 0);
        order.setInteger (Fields.ExpireDateTime, 0);
        order.setInteger (Fields.DisplayQty, 0);
        order.setInteger (Fields.Capacity, 1);
        order.setInteger (Fields.OrderSubType, 0);
        order.setInteger (Fields.Anonymity, 0);
        order.setDouble (Fields.StopPrice, 0.0);
        order.setInteger (Fields.PassiveOnlyOrder, 0);
        order.setInteger (Fields.ClientID, 1234);
        order.setInteger (Fields.InvestmentDecisionMaker, 0);
        order.setInteger (Fields.MinimumQuantity, 0);
        order.setInteger (Fields.ExecutingTrader, 7676);

        log.info("sending-input-order");
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

        log.info("destroying connector");
    }
    
    public static void main(String[] args) {
        example instance = new example();
        instance.doMain();
    }
}
