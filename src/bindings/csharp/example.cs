using Neueda.Properties;
using Neueda.Fosdk;
using Neueda.Codecs;
using Neueda.Log;
using Neueda.Cdr;

using System;
using System.Threading;


namespace Example
{
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
            this.logger.log (LogSeverity.level.INFO, "session logged on...");
        }

        public override void onLoggingOn (Cdr msg) {
            this.logger.log (LogSeverity.level.INFO, "session logging on...");

            // set username and password
            msg.setString (codecBinding.UserName, this.username);
            msg.setString (codecBinding.Password, this.password);
        }

        public override bool onError (String errorMessage) {
            this.logger.log (LogSeverity.level.ERROR, "session err: " + errorMessage);

            // return true to attempt reconnection
            return true;
        }

        public override void onLoggedOn (ulong seqno, Cdr msg) {
            this.logger.log (LogSeverity.level.INFO, "session logged on...");
        }

        public override void onLoggedOff (ulong seqno, Cdr msg) {
            this.logger.log (LogSeverity.level.INFO, "session logged off...");
        }

        public override void onGap (ulong expected, ulong recieved) {
            this.logger.log (LogSeverity.level.INFO, "gap deteched expected [" + expected + "] got [" + recieved + "]");
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
    
        public override void onAdmin (ulong seqno, Cdr msg) {
            this.logger.log (LogSeverity.level.INFO, "onAdmin msg...");
            this.logger.log (LogSeverity.level.INFO, msg.toString ());
        }

        public override void onOrderAck (ulong seqno, Cdr msg) {
            this.logger.log (LogSeverity.level.INFO, "onOrderAck msg...");
            this.logger.log (LogSeverity.level.INFO, msg.toString ());

            // get order id from ack
            String orderId = msg.getString (codecBinding.OrderID);

            // send modify
            Cdr modify = new Cdr();
            modify.setString (codecBinding.OriginalClientOrderID, "myorder");
            modify.setString (codecBinding.ClientOrderID, "myorder1");
            modify.setString (codecBinding.OrderID, orderId);
            modify.setInteger (codecBinding.InstrumentID, 133215); // VOD.L
            modify.setInteger (codecBinding.OrderQty, 2000);
            modify.setInteger (codecBinding.OrderType, 2);
            modify.setDouble (codecBinding.LimitPrice, 1234.56);
            modify.setInteger (codecBinding.Side, 1);
            modify.setString (codecBinding.Account, "account");
            modify.setInteger (codecBinding.ExpireDateTime, 0);
            modify.setInteger (codecBinding.DisplayQty, 0);
            modify.setDouble (codecBinding.StopPrice, 0.0);
            modify.setInteger (codecBinding.PassiveOnlyOrder, 0);
            modify.setInteger (codecBinding.ClientID, 1234);
            modify.setInteger (codecBinding.MinimumQuantity, 0);
            modify.setInteger (codecBinding.PassiveOnlyOrder, 0);
            modify.setInteger (codecBinding.ReservedField1, 0);
            modify.setInteger (codecBinding.ReservedField2, 0);
            modify.setInteger (codecBinding.ReservedField3, 0);
            modify.setInteger (codecBinding.ReservedField4, 0);

            if (!this.gwc.sendModify (modify))
                this.logger.log (LogSeverity.level.ERROR, "failed to send modify");
        }

        public override void onOrderRejected (ulong seqno, Cdr msg) {
            this.logger.log (LogSeverity.level.INFO, "onOrderRejected msg...");
            this.logger.log (LogSeverity.level.INFO, msg.toString ());
        }

        public override void onOrderDone (ulong seqno, Cdr msg) {
            this.logger.log (LogSeverity.level.INFO, "onOrderDone msg...");
            this.logger.log (LogSeverity.level.INFO, msg.toString ());
        }

        public override void onOrderFill (ulong seqno, Cdr msg) {
            this.logger.log (LogSeverity.level.INFO, "onOrderFill msg...");
            this.logger.log (LogSeverity.level.INFO, msg.toString ());
        }

        public override void onModifyAck (ulong seqno, Cdr msg) {
            this.logger.log (LogSeverity.level.INFO, "onModifyAck msg...");
            this.logger.log (LogSeverity.level.INFO, msg.toString ());

            // cancel order
            String orderId = msg.getString (codecBinding.OrderID);

            Cdr cancel = new Cdr ();
            cancel.setString (codecBinding.OriginalClientOrderID, "myorder1");
            cancel.setString (codecBinding.ClientOrderID, "myorder2");
            cancel.setString (codecBinding.OrderID, orderId);
            cancel.setInteger (codecBinding.InstrumentID, 133215); // VOD.L
            cancel.setInteger (codecBinding.Side, 1);
            cancel.setString (codecBinding.RfqID, "XXXX");

            cancel.setInteger (codecBinding.ReservedField1, 0);
            cancel.setInteger (codecBinding.ReservedField2, 0);

            if (!this.gwc.sendCancel (cancel))
                this.logger.log (LogSeverity.level.INFO, "failed to send cancel myorder1...");
        }

        public override void onModifyRejected (ulong seqno, Cdr msg) {
            this.logger.log (LogSeverity.level.INFO, "oModifyRejected msg...");
            this.logger.log (LogSeverity.level.INFO, msg.toString ());
        }

        public override void onCancelRejected (ulong seqno, Cdr msg) {
            this.logger.log (LogSeverity.level.INFO, "onCacnelRejected msg...");
            this.logger.log (LogSeverity.level.INFO, msg.toString ());
        }

        public override void onMsg (ulong seqno, Cdr msg) {
            this.logger.log (LogSeverity.level.INFO, "onMsg msg...");
            this.logger.log (LogSeverity.level.INFO, msg.toString ());
        }
    }

    class example
    {
        public example() { }

        public void doMain()
        {
            LogService logService = LogService.get ();
            Properties logProps = new Properties ();
            logProps.setProperty ("ls.console.level", "debug");
            logProps.setProperty ("ls.console.color", "true");
            logService.configure (logProps);

            Properties props = new Properties ("gwc", "millennium", "sim");
            props.setProperty ("real_time_host", "127.0.0.1:9899");
            props.setProperty ("recovery_host", "127.0.0.1:10000");
            props.setProperty ("venue", "lse");

            Logger log = LogService.getLogger ("MILLENIUM_TEST");
            gwcConnector gwc = gwcConnectorFactory.get (log, "millennium", props);
            if (gwc == null) {
                Console.WriteLine ("failed to get connector...");
                return;
            }

            SessionCallbacks sessionCbs = new SessionCallbacks(gwc, log, "USER1", "PASS1");
            MessageCallbacks messageCbs = new MessageCallbacks(gwc, log);

            log.log (LogSeverity.level.INFO, "initialising connector...");
            if (!gwc.init (sessionCbs, messageCbs, props)) {
                Console.WriteLine ("failed to initialise connector...");
                return;
            }

            log.log (LogSeverity.level.INFO, "starting connector...");
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
            order.setString (codecBinding.ClientOrderID, "myorder");
            order.setInteger (codecBinding.InstrumentID, 133215); // VOD.L
            order.setInteger (codecBinding.AutoCancel, 1);

            order.setString (codecBinding.TraderID, "TX1");
            order.setString (codecBinding.Account, "account");
            order.setInteger (codecBinding.ClearingAccount, 1);
            order.setInteger (codecBinding.FXMiFIDFlags, 0);
            order.setInteger (codecBinding.PartyRoleQualifiers, 0);
            order.setInteger (codecBinding.ExpireDateTime, 0);
            order.setInteger (codecBinding.DisplayQty, 0);
            order.setInteger (codecBinding.Capacity, 1);
            order.setInteger (codecBinding.OrderSubType, 0);
            order.setInteger (codecBinding.Anonymity, 0);
            order.setDouble (codecBinding.StopPrice, 0.0);
            order.setInteger (codecBinding.PassiveOnlyOrder, 0);
            order.setInteger (codecBinding.ClientID, 1234);
            order.setInteger (codecBinding.InvestmentDecisionMaker, 0);
            order.setInteger (codecBinding.MinimumQuantity, 0);
            order.setInteger (codecBinding.ExecutingTrader, 7676);

            log.log (LogSeverity.level.INFO, "sending-input-order");
            if (!gwc.sendOrder (order)) {
                Console.WriteLine ("failed to send order myorder...");
                return;
            }

            Thread.Sleep(5000);
            gwc.stop ();
            Thread.Sleep(2000);


            log.log (LogSeverity.level.INFO, "destroying connector");
            gwc.Dispose();
        }

        static void Main(string[] args)
        {
            example instance = new example();
            instance.doMain();
        }
    }
}
