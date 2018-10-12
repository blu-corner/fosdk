import Config
import Fosdk
import Codec
import Cdr
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

    def onRawMsg (self, seqno, data, length):
        self.logger.log (Log.INFO, "onRawMsg msg...")
        self.logger.log (Log.INFO, "type: %s - length: %i" % (repr(data), length))

        # this is hack to get around a bug in swig typemap in
        # a director callback for python only
        buf = Codec.Buffer (None, False)
        buf.setRaw(data, length, False)
        lseHeader = Codec.lseCodec.bufferToHeader(buf)

        if lseHeader.mMessageType == '3':
            self.logger.log (Log.INFO, "recvieved: reject")
        elif lseHeader.mMessageType == '8':
            self.logger.log (Log.INFO, "recvieved: exec-report")
            
            execReport = Codec.lseCodec.bufferToExecutionReport(buf)
            self.logger.log (Log.INFO, "exec-report order-id: %s" % execReport.getOrderID())
            self.logger.log (Log.INFO, "exec-report client-order-id: %s" % execReport.getClientOrderID())
            
        else :
            self.logger.log (Log.INFO, "recvieved: exec-report %s" % lseHeader.mMessageType)


def signal_handler(signal, frame):
    print('caught sigint')
    sys.exit(1)


def main():
    logService = Log.LogService.get ()
    pyLogHandler = PythonLogHandler()
    logService.addHandler (pyLogHandler)

    properties = Config.RawProperties()
    props = Config.Properties(properties, "gwc", "millennium", "sim")
    props.setProperty ("real_time_host", "127.0.0.1:9899")
    props.setProperty ("recovery_host", "127.0.0.1:10000")
    props.setProperty ("venue", "lse")
    props.setProperty ("enable_raw_messages", "yes");

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
    newOrder = Codec.LseNewOrder()
    newOrder.mHeader.mMessageType = 'D'
    newOrder.mHeader.mStartOfMessage = 4
    newOrder.mHeader.mMessageLength = Codec.LseNewOrder.getRawSize() - Codec.LseHeader.getRawSize() - 1;
    newOrder.setLimitPrice(1234)
    newOrder.setOrderQty(1000)
    newOrder.setTIF(10)
    newOrder.setSide(1)
    newOrder.setOrderType(2)
    newOrder.setClientOrderID("myorder")
    newOrder.setInstrumentID(133215)
    newOrder.setAutoCancel(1)
    newOrder.setTraderID("TX1")
    newOrder.setAccount("account")
    newOrder.setClearingAccount(1)
    newOrder.setCapacity(1)
    newOrder.setClientID(1234)
    newOrder.setExecutingTrader(7676)

    log.log (Log.INFO, "sending-input-order")
    if not gwc.sendBuffer (newOrder.getBuffer ()):
        sys.exit ("failed to send order myorder...")

    time.sleep (5)
    gwc.stop ()
    time.sleep (2)
    log.log (Log.INFO, "destroying connector")
    logService.removeHandler (pyLogHandler)


if __name__ == "__main__":
    logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)
    main()
