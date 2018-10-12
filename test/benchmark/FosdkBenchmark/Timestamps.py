import datetime
import struct
import os


class TimestampsFile(tuple):

    _method = None
    _cpuFrequency = None

    @property
    def timestampMethod(self):
        return self._method

    @property
    def cpuFrequency(self):
        return self._cpuFrequency

    def getDeltas(self):
        return tuple(map(lambda x,y: x - y, self[1:], self[:-1]))

    @staticmethod
    def load(dataFile):
        with open(dataFile, "rb") as fd:
            numSamples = struct.unpack("I", fd.read(4))[0]
            cpuFrequency = struct.unpack("Q", fd.read(8))[0]
            methodStringLength = struct.unpack("I", fd.read(4))[0]
            methodString = struct.unpack("{0}s".format(methodStringLength),
                                         fd.read(methodStringLength))[0]
            numPadding = struct.unpack("I", fd.read(4))[0]
            padding = struct.unpack("{0}s".format(numPadding), fd.read(numPadding))

            instance = TimestampsFile(map(lambda i: struct.unpack("Q", fd.read(8))[0], range(numSamples)))
            instance._method = methodString
            instance._cpuFrequency = cpuFrequency
            return instance
