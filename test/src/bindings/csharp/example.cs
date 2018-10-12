using Neueda.Config;
using Neueda.Fosdk;
using Neueda.Codec;
using Neueda.Log;
using Neueda.Cdr;

using System;
using System.Threading;


namespace Example
{
    public class LocalLogHandler : LogHandler
    {
        public LocalLogHandler () : base() { }

        public override LogSeverity getLevel() {
            return LogSeverity.DEBUG;
        }

        public override string getFormat() {
            return "{severity} {name} {time} {message}";
        }

        public override void handle (LogSeverity severity,
                                     string name,
                                     SWIGTYPE_p_tm tm_time,
                                     SWIGTYPE_p_timeval tv,
                                     string message,
                                     uint message_len)
        {
            string m = LogHandler.toString (getFormat (),
                                            severity,
                                            name,
                                            tm_time,
                                            tv,
                                            message,
                                            message_len);
            Console.WriteLine (m);
        }
    }
    
    public class SessionCallbacks : gwcSessionCallbacks
    {
        private gwcConnector gwc;
        private Logger logger;
        private String username;
        private String password;
        
        public SessionCallbacks(gwcConnector gwc, Logger logger, String username, String password)
            : base()
        {
            this.gwc = gwc;
            this.logger = logger;
            this.username = username;
            this.password = password;
        }

        public override void onConnected () {
            this.logger.log (LogSeverity.INFO, "session logged on...");
        }

        public override void onLoggingOn (cdr msg) {
            this.logger.log (LogSeverity.INFO, "session logging on...");

            // set username and password
            msg.setString (Codec.UserName, this.username);
            msg.setString (Codec.Password, this.password);
        }

        public override bool onError (String errorMessage) {
            this.logger.log (LogSeverity.ERROR, "session err: " + errorMessage);

            // return true to attempt reconnection
            return true;
        }

        public override void onLoggedOn (ulong seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "session logged on...");
        }

        public override void onLoggedOff (ulong seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "session logged off...");
        }

        public override void onGap (ulong expected, ulong recieved) {
            this.logger.log (LogSeverity.INFO, "gap deteched expected [" + expected + "] got [" + recieved + "]");
        }
    }

    public class MessageCallbacks : gwcMessageCallbacks
    {
        private gwcConnector gwc;
        private Logger logger;
        
        public MessageCallbacks(gwcConnector gwc, Logger logger)
            : base()
        {
            this.gwc = gwc;
            this.logger = logger;
        }
    
        public override void onAdmin (ulong seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "onAdmin msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());
        }

        public override void onOrderAck (ulong seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "onOrderAck msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());

            // get order id from ack
            String orderId = msg.getString (Codec.OrderID);
        
            // send modify
            cdr modify = new cdr();
            modify.setString (Codec.OriginalClientOrderID, "myorder");
            modify.setString (Codec.ClientOrderID, "myorder1");
            modify.setString (Codec.OrderID, orderId);
            modify.setInteger (Codec.InstrumentID, 133215); // VOD.L
            modify.setInteger (Codec.OrderQty, 2000);
            modify.setInteger (Codec.OrderType, 2);
            modify.setDouble (Codec.LimitPrice, 1234.56);
            modify.setInteger (Codec.Side, 1);
            modify.setString (Codec.Account, "account");
            modify.setInteger (Codec.ExpireDateTime, 0);
            modify.setInteger (Codec.DisplayQty, 0);
            modify.setDouble (Codec.StopPrice, 0.0);
            modify.setInteger (Codec.PassiveOnlyOrder, 0);
            modify.setInteger (Codec.ClientID, 1234);
            modify.setInteger (Codec.MinimumQuantity, 0);
            modify.setInteger (Codec.PassiveOnlyOrder, 0);
            modify.setInteger (Codec.ReservedField1, 0);
            modify.setInteger (Codec.ReservedField2, 0);
            modify.setInteger (Codec.ReservedField3, 0);
            modify.setInteger (Codec.ReservedField4, 0);
    
            if (!this.gwc.sendModify (modify))
                this.logger.log (LogSeverity.ERROR, "failed to send modify");
        }

        public override void onOrderRejected (ulong seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "onOrderRejected msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());
        }

        public override void onOrderDone (ulong seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "onOrderDone msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());
        }

        public override void onOrderFill (ulong seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "onOrderFill msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());
        }

        public override void onModifyAck (ulong seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "onModifyAck msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());

            // cancel order
            String orderId = msg.getString (Codec.OrderID);
        
            cdr cancel = new cdr ();
            cancel.setString (Codec.OriginalClientOrderID, "myorder1");
            cancel.setString (Codec.ClientOrderID, "myorder2");
            cancel.setString (Codec.OrderID, orderId);
            cancel.setInteger (Codec.InstrumentID, 133215); // VOD.L
            cancel.setInteger (Codec.Side, 1);
            cancel.setString (Codec.RfqID, "XXXX");

            cancel.setInteger (Codec.ReservedField1, 0);
            cancel.setInteger (Codec.ReservedField2, 0);
    
            if (!this.gwc.sendCancel (cancel))
                this.logger.log (LogSeverity.INFO, "failed to send cancel myorder1...");
        }

        public override void onModifyRejected (ulong seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "oModifyRejected msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());
        }

        public override void onCancelRejected (ulong seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "onCacnelRejected msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());
        }

        public override void onMsg (ulong seqno, cdr msg) {
            this.logger.log (LogSeverity.INFO, "onMsg msg...");
            this.logger.log (LogSeverity.INFO, msg.toString ());
        }
    }
    
    class example
    {
        public example() { }

        public void doMain()
        {
            RawProperties properties = new RawProperties();
            
            LogService logService = LogService.get ();
            LocalLogHandler logHandler = new LocalLogHandler ();
            logService.addHandler (logHandler);
            
            Properties props = new Properties(properties, "gwc", "millennium", "sim");
            props.setProperty ("real_time_host", "127.0.0.1:9899");
            props.setProperty ("recovery_host", "127.0.0.1:10000");
            props.setProperty ("venue", "lse")

            Logger log = logService.getLogger ("MILLENIUM_TEST");
            gwcConnector gwc = gwcConnectorFactory.get (log, "millennium", props);
            if (gwc == null) {
                Console.WriteLine ("failed to get connector...");
                return;
            }
            
            SessionCallbacks sessionCbs = new SessionCallbacks(gwc, log, "USER1", "PASS1");
            MessageCallbacks messageCbs = new MessageCallbacks(gwc, log);
            
            log.log (LogSeverity.INFO, "initialising connector...");
            if (!gwc.init (sessionCbs, messageCbs, props)) {
                Console.WriteLine ("failed to initialise connector...");
                return;
            }
            
            log.log (LogSeverity.INFO, "starting connector...");
            if (!gwc.start (false)) {
                Console.WriteLine ("failed to initialise connector...");
                return;
            }
            
            // wait for logon - blocks untill state change
            gwc.waitForLogon ();
            
            // send order
            gwcOrder order = new gwcOrder();
            order.setPrice (1234.45);
            order.setQty (1000);
            order.setTif (gwcTif.GWC_TIF_DAY);
            order.setSide (gwcSide.GWC_SIDE_BUY);
            order.setOrderType (gwcOrderType.GWC_ORDER_TYPE_LIMIT);
            order.setString (Codec.ClientOrderID, "myorder");
            order.setInteger (Codec.InstrumentID, 133215); // VOD.L
            order.setInteger (Codec.AutoCancel, 1);
            
            order.setString (Codec.TraderID, "TX1");
            order.setString (Codec.Account, "account");
            order.setInteger (Codec.ClearingAccount, 1);
            order.setInteger (Codec.FXMiFIDFlags, 0);
            order.setInteger (Codec.PartyRoleQualifiers, 0);
            order.setInteger (Codec.ExpireDateTime, 0);
            order.setInteger (Codec.DisplayQty, 0);
            order.setInteger (Codec.Capacity, 1);
            order.setInteger (Codec.OrderSubType, 0);
            order.setInteger (Codec.Anonymity, 0);
            order.setDouble (Codec.StopPrice, 0.0);
            order.setInteger (Codec.PassiveOnlyOrder, 0);
            order.setInteger (Codec.ClientID, 1234);
            order.setInteger (Codec.InvestmentDecisionMaker, 0);
            order.setInteger (Codec.MinimumQuantity, 0);
            order.setInteger (Codec.ExecutingTrader, 7676);
            
            log.log (LogSeverity.INFO, "sending-input-order");
            if (!gwc.sendOrder (order)) {
                Console.WriteLine ("failed to send order myorder...");
                return;
            }
            
            Thread.Sleep(5000);
            gwc.stop ();
            Thread.Sleep(2000);
            
            
            log.log (LogSeverity.INFO, "destroying connector");
            gwc.Dispose();
            logService.removeHandler (logHandler);
        }
        
        static void Main(string[] args)
        {
            example instance = new example();
            instance.doMain();
        }
    }
}
