import CommonDataRepresentation as Cdr
import Codecs as Codec
import Config
import Fosdk
import Log

import logging
import signal
import time
import sys


# uses python logging backend - optional
logger = logging.getLogger("python-millennium-connector")


class PythonLogHandler(Log.LogHandler):

    def __init__(self):
        super(PythonLogHandler, self).__init__()

    @property
    def log(self):
        return logger

    def setup(self, errorMessage):
        return True

    def getFormat(self):
        return "{message}"

    # ensure all levels are enabled to let python logging backend config to decide
    def isLevelEnabled(self, level):
        return True

    def handle(self, severity, name, time, tv, message, messageLength):
        formatted = Log.LogHandler.toString (self.getFormat (),
                                             severity,
                                             name,
                                             time,
                                             tv,
                                             message,
                                             messageLength)
        if severity == Log.DEBUG:
            self.log.debug(formatted)
        elif severity == Log.INFO:
            self.log.info(formatted)
        elif severity == Log.WARNING:
            self.log.warn(formatted)
        elif severity == Log.ERROR:
            self.log.error(formatted)
        elif severity == Log.FATAL:
            self.log.error(formatted)


class SessionCallbacks(Fosdk.gwcSessionCallbacks):
    
    def __init__(self, gwc, logger, username, password):
        super(SessionCallbacks, self).__init__()
        self._gwc = gwc
        self._log = logger
        self._username = username
        self._password = password

    @property
    def logger(self):
        return self._log

    @property
    def gwc(self):
        return self._gwc

    def onConnected (self):
        self.logger.log (Log.INFO, "session logged on...")

    def onLoggingOn (self, msg):
        self.logger.log (Log.INFO, "session logging on...")

        # set username and password
        msg.setString (Codec.UserName, self._username);
        msg.setString (Codec.Password, self._password);

    def onError (self, errorMessage):
        self.logger.log (Log.ERROR, "session err [{0}]".format(errorMessage));

        # return true to attempt reconnection
        return True

    def onLoggedOn (self, seqno, msg):
        self.logger.log (Log.INFO, "session logged on...")

    def onLoggedOff (self, seqno, msg):
        self.logger.log (Log.INFO, "session logged off...")

    def onGap (self, expected, recieved):
        self.log.info ("gap detected expected [{0}] got [{1}] " \
                       .format(expected, recieved))


class MessageCallbacks(Fosdk.gwcMessageCallbacks):
    
    def __init__(self, gwc, logger):
        super(MessageCallbacks, self).__init__()
        self._gwc = gwc
        self._log = logger

    @property
    def logger(self):
        return self._log

    @property
    def gwc(self):
        return self._gwc
    
    def onAdmin (self, seqno, msg):
        self.logger.log (Log.INFO, "onAdmin msg...")
        self.logger.log (Log.INFO, "{0}".format(msg.toString ()))

    def onOrderAck (self, seqno, msg):
        self.logger.log (Log.INFO, "onOrderAck msg...")
        self.logger.log (Log.INFO, "{0}".format(msg.toString ()))

        # get order id from ack
        orderId = msg.getString (Codec.OrderID)
        
        # send modify
        modify = Cdr.cdr()
        modify.setString (Codec.OriginalClientOrderID, "myorder")
        modify.setString (Codec.ClientOrderID, "myorder1")
        modify.setString (Codec.OrderID, orderId)
        modify.setInteger (Codec.InstrumentID, 133215) # VOD.L
        modify.setInteger (Codec.OrderQty, 2000)
        modify.setInteger (Codec.OrderType, 2)
        modify.setDouble (Codec.LimitPrice, 1234.56)
        modify.setInteger (Codec.Side, 1)
        modify.setString (Codec.Account, "account")
        modify.setInteger (Codec.ExpireDateTime, 0)
        modify.setInteger (Codec.DisplayQty, 0)
        modify.setDouble (Codec.StopPrice, 0.0)
        modify.setInteger (Codec.PassiveOnlyOrder, 0)
        modify.setInteger (Codec.ClientID, 1234)
        modify.setInteger (Codec.MinimumQuantity, 0)
        modify.setInteger (Codec.PassiveOnlyOrder, 0)
        modify.setInteger (Codec.ReservedField1, 0)
        modify.setInteger (Codec.ReservedField2, 0)
        modify.setInteger (Codec.ReservedField3, 0)
        modify.setInteger (Codec.ReservedField4, 0)
    
        if not self.gwc.sendModify (modify):
            self.logger.log (Log.Error, "failed to send modify")

    def onOrderRejected (self, seqno, msg):
        self.logger.log (Log.INFO, "onOrderRejected msg...")
        self.logger.log (Log.INFO, "{0}".format(msg.toString ()))

    def onOrderDone (self, seqno, msg):
        self.logger.log (Log.INFO, "onOrderDone msg...")
        self.logger.log (Log.INFO, "{0}".format(msg.toString ()))

    def onOrderFill (self, seqno, msg):
        self.logger.log (Log.INFO, "onOrderFill msg...")
        self.logger.log (Log.INFO, "{0}".format(msg.toString ()))

    def onModifyAck (self, seqno, msg):
        self.logger.log (Log.INFO, "onModifyAck msg...")
        self.logger.log (Log.INFO, "{0}".format(msg.toString ()))

        # cancel order
        orderId = msg.getString (Codec.OrderID)
        
        cancel = Cdr.cdr ()
        cancel.setString (Codec.OriginalClientOrderID, "myorder1")
        cancel.setString (Codec.ClientOrderID, "myorder2")
        cancel.setString (Codec.OrderID, orderId)
        cancel.setInteger (Codec.InstrumentID, 133215) # VOD.L
        cancel.setInteger (Codec.Side, 1)
        cancel.setString (Codec.RfqID, "XXXX")

        cancel.setInteger (Codec.ReservedField1, 0)
        cancel.setInteger (Codec.ReservedField2, 0)
    
        if not self.gwc.sendCancel (cancel):
            self.logger.log (Log.INFO, "failed to send cancel myorder1...")

    def onModifyRejected (self, seqno, msg):
        self.logger.log (Log.INFO, "oModifyRejected msg...")
        self.logger.log (Log.INFO, "{0}".format(msg.toString ()))

    def onCancelRejected (self, seqno, msg):
        self.logger.log ("onCacnelRejected msg...")
        self.logger.log (Log.INFO, "{0}".format(msg.toString ()))

    def onMsg (self, seqno, msg):
        self.logger.log ("onMsg msg...")
        self.logger.log (Log.INFO, "{0}".format(msg.toString ()))


def signal_handler(signal, frame):
    print('caught sigint')
    sys.exit(1)


def main():
    properties = Config.RawProperties()
    pyLogHandler = PythonLogHandler()
    
    logService = Log.LogService.get ()
    logService.addHandler (pyLogHandler)

    props = Config.Properties(properties, "gwc", "millennium", "sim")
    props.setProperty ("real_time_host", "127.0.0.1:9899")
    props.setProperty ("recovery_host", "127.0.0.1:10000")
    props.setProperty ("venue", "lse")

    log = logService.getLogger ("MILLENIUM_TEST")
    gwc = Fosdk.gwcConnectorFactory.get (log, "millennium", props)
    if gwc == None:
        sys.exit ("failed to get connector...")

    sessionCbs = SessionCallbacks(gwc, log, "USER1", "PASS1")
    messageCbs = MessageCallbacks(gwc, log)

    # catch sigint
    signal.signal(signal.SIGINT, signal_handler)

    log.log (Log.INFO, "initialising connector...")
    if not gwc.init (sessionCbs, messageCbs, props):
        sys.exit ("failed to initialise connector...")

    log.log (Log.INFO, "starting connector...")
    if not gwc.start (False):
        sys.exit ("failed to initialise connector...")

    # wait for logon - blocks untill state change
    gwc.waitForLogon ()
 
    # send order
    order = Fosdk.gwcOrder()
    order.setPrice (1234.45)
    order.setQty (1000)
    order.setTif (Fosdk.GWC_TIF_DAY)
    order.setSide (Fosdk.GWC_SIDE_BUY)
    order.setOrderType (Fosdk.GWC_ORDER_TYPE_LIMIT)
    order.setString (Codec.ClientOrderID, "myorder")
    order.setInteger (Codec.InstrumentID, 133215) # VOD.L
    order.setInteger (Codec.AutoCancel, 1)

    order.setString (Codec.TraderID, "TX1")
    order.setString (Codec.Account, "account")
    order.setInteger (Codec.ClearingAccount, 1)
    order.setInteger (Codec.FXMiFIDFlags, 0)
    order.setInteger (Codec.PartyRoleQualifiers, 0)
    order.setInteger (Codec.ExpireDateTime, 0)
    order.setInteger (Codec.DisplayQty, 0)
    order.setInteger (Codec.Capacity, 1)
    order.setInteger (Codec.OrderSubType, 0)
    order.setInteger (Codec.Anonymity, 0)
    order.setDouble (Codec.StopPrice, 0.0)
    order.setInteger (Codec.PassiveOnlyOrder, 0)
    order.setInteger (Codec.ClientID, 1234)
    order.setInteger (Codec.InvestmentDecisionMaker, 0)
    order.setInteger (Codec.MinimumQuantity, 0)
    order.setInteger (Codec.ExecutingTrader, 7676)

    log.log (Log.INFO, "sending-input-order")
    if not gwc.sendOrder (order):
        sys.exit ("failed to send order myorder...")

    time.sleep (5)
    gwc.stop ()
    time.sleep (2)
    log.log (Log.INFO, "destroying connector")
    logService.removeHandler (pyLogHandler)


if __name__ == "__main__":
    logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)
    main()
