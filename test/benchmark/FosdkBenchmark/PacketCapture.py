#!/usr/bin/env python

from scapy.all import *
import sys


class PCAPFile:

    def __init__(self, sent, received):
        self._sent = sent
        self._received = received

    @property
    def sent(self):
        return self._sent

    @property
    def received(self):
        return self._received

    @property
    def sent_timesamps(self):
        return tuple(map(lambda i: i.time, self.sent))

    @property
    def received_timesamps(self):
        return tuple(map(lambda i: i.time, self.received))

    @staticmethod
    def load(dataFile, destinationPort):
        packets = rdpcap(dataFile)
        sent = tuple(filter(lambda p: p.dport == destinationPort, packets))
        received = tuple(filter(lambda p: p.sport == destinationPort, packets))
        return PCAPFile(sent, received)


def main():
    pcap = PCAPFile.load(sys.argv[1], 9999)
    print(pcap.sent[0])
    print(pcap.received[0])
    print len(pcap.sent)
    print len(pcap.received)
    print pcap.sent_timesamps
    print pcap.received_timesamps


if __name__ == "__main__":
    main()
