#!/usr/bin/env python
from Timestamps import TimestampsFile
# from PacketCapture import PCAPFile
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import optparse
import logging
import shutil
import json
import csv
import sys
import os

logging.basicConfig(format='%(asctime)s - %(levelname)s - %(message)s', level=logging.INFO)


class Analyzer:

    def __init__(self, results, output):
        self._results = results
        self._output = output

        self._acked_ts_file = os.path.join(self._results, "order-acked-timestamps.dat")
        if self._acked_ts_file is None:
            sys.exit("Timestamps file missing for acked orders")

        self._entry_ts_file = os.path.join(self._results, "order-entry-called-timestamps.dat")
        if self._entry_ts_file is None:
            sys.exit("Timestamps file missing for entry orders")

        self._pcap_file = os.path.join(self._results, "traffic.pcap")
        if self.pcap_file is None:
            sys.exit("PCAP file missing")

    @property
    def logger(self):
        logger = logging.getLogger(__name__)
        return logger

    @property
    def results_folder(self):
        return self._results

    @property
    def output(self):
        return self._output

    @property
    def acked_ts_file(self):
        return self._acked_ts_file

    @property
    def entry_ts_file(self):
        return self._entry_ts_file

    @property
    def entry_timestamps(self):
        return TimestampsFile.load(self._entry_ts_file)
    
    @property
    def acked_timestamps(self):
        return TimestampsFile.load(self._acked_ts_file)

    @property
    def pcap_file(self):
        return self._pcap_file

    def move_files(self):
        if os.path.isdir(self.output):
            pass
        else:
            os.makedirs(self.output)
        source = os.listdir(os.getcwd())
        for files in source:
            if files.endswith('.png') or files.endswith('.json') or files.endswith('.csv'):
                shutil.move(os.path.join(os.getcwd(), files), os.path.join(output_dir, files))

    @staticmethod
    def write_deltas_to_csv(loaded_ts_file, filename):
        all_deltas = loaded_ts_file.getDeltas()
        with open("{0}".format(filename), "wb") as delta_csv:
            writer = csv.writer(delta_csv)
            writer.writerow(["TimeStamp", "Delta"])
            writer.writerow([loaded_ts_file[0], "0"])
            for i in range(len(loaded_ts_file)):
                try:
                    writer.writerow([loaded_ts_file[i + 1], all_deltas[i]])
                except IndexError:
                    continue

    @staticmethod
    def write_roundtrips_to_csv(entry_ts, ack_ts, roundtrips, filename):
        with open("{0}".format(filename), "wb") as delta_csv:
            writer = csv.writer(delta_csv)
            writer.writerow(["Entry ts", "Ack ts", "Delta"])
            for i in range(len(entry_ts)):
                writer.writerow([entry_ts[i], ack_ts[i], roundtrips[i]])

    @staticmethod
    def write_to_json(data, filename):
        with open(filename, 'w') as outfile:
            print >> outfile, json.dumps(data, sort_keys=True, indent=4, separators=(',', ': '))

    @staticmethod
    def get_round_trip_values(order_entry, order_acked):
        return tuple(map(lambda x, y: x - y, order_acked, order_entry))

    def process_round_trip_to_disk_and_graph(self, entry, acked):
        round_dicts = self.entry_timestamps
        round_trips = self.get_round_trip_values(entry, acked)
        self.logger.info(("** ROUND:", round_dicts.timestampMethod))
        self.logger.info(("min-round-trips:", min(round_trips)))
        self.logger.info(("max-round-trips:", max(round_trips)))
        self.logger.info(("avg-round-trips:", np.average(round_trips)))
        self.logger.info(("median-round-trips:", np.median(round_trips)))
        round_data = {}
        round_data["method"] = round_dicts.timestampMethod
        round_data["cpu_frequency"] = round_dicts.cpuFrequency
        round_data["min"] = min(round_trips)
        round_data["max"] = max(round_trips)
        round_data["avg"] = np.average(round_trips)
        round_data["median"] = np.median(round_trips)
        round_data["deviation"] = np.std(round_trips)

        self.write_roundtrips_to_csv(self.entry_timestamps, self.acked_timestamps, round_trips, "round_trip-latencies.csv")
        self.write_to_json(round_data, "round-trip-consolidated-results.json")
        self.plot_graph(round_trips, "Time Stamp Latencies", "Latencies", "Round Trip", "RoundTrips.png")

    @staticmethod
    def plot_graph(x_axis, x_label, y_label, title, graph_name):
        plt.plot(x_axis)
        plt.ylabel(y_label)
        plt.xlabel(x_label)
        plt.title(title)
        plt.savefig(graph_name, bbox_inches='tight')
        plt.close()

    def process_order_entry(self):
        entry_ts = self.entry_timestamps
        entryDeltas = entry_ts.getDeltas()
        self.plot_graph(entryDeltas, "Deltas", "ms", "Entry Deltas", "EntryDeltas.png")

        self.logger.info(("** ENTRY:", entry_ts.timestampMethod))
        self.logger.info(("min-entry-latency:", min(entryDeltas)))
        self.logger.info(("max-entry-latency:", max(entryDeltas)))
        self.logger.info(("avg-entry-latency:", sum(entryDeltas) / len(entryDeltas)))
        self.logger.info(("median-entry-latency:", np.median(entryDeltas)))
        order_data = {}
        order_data["method"] = entry_ts.timestampMethod
        order_data["cpu_frequency"] = entry_ts.cpuFrequency
        order_data["min"] = min(entryDeltas)
        order_data["max"] = max(entryDeltas)
        order_data["avg"] = sum(entryDeltas) / len(entryDeltas)
        order_data["median"] = np.median(entryDeltas)
        order_data["deviation"] = np.std(entryDeltas)

        self.write_deltas_to_csv(entry_ts, "order-entry-deltas.csv")
        self.write_to_json(order_data, "order-entry-consolidated-results.json")
        return entry_ts

    def process_order_acked(self):
        acked_ts = self.acked_timestamps
        ackedDeltas = acked_ts.getDeltas()
        self.plot_graph(ackedDeltas, "deltas", "ms", "Acked Deltas", "AckedDeltas.png")

        self.logger.info(("** ACKED:", acked_ts.timestampMethod))
        self.logger.info(("min-acked-latency:", min(ackedDeltas)))
        self.logger.info(("max-acked-latency:", max(ackedDeltas)))
        self.logger.info(("avg-acked-latency:", sum(ackedDeltas) / len(ackedDeltas)))
        self.logger.info(("median-acked-latency:", np.median(ackedDeltas)))
        acked_data = {}
        acked_data["method"] = acked_ts.timestampMethod
        acked_data["cpu_frequency"] = acked_ts.cpuFrequency
        acked_data["min"] = min(ackedDeltas)
        acked_data["max"] = max(ackedDeltas)
        acked_data["avg"] = sum(ackedDeltas) / len(ackedDeltas)
        acked_data["median"] = np.median(ackedDeltas)
        acked_data["deviation"] = np.std(ackedDeltas)

        self.write_deltas_to_csv(acked_ts, "order-acked-deltas.csv")
        self.write_to_json(acked_data, "order-acked-consolidated-results.json")
        return acked_ts


if __name__ == "__main__":
    parser = optparse.OptionParser()
    parser.add_option("--results", dest="results_dir", help="Path to results directory")
    parser.add_option("--output_dir", dest="output_dir", help="Path to output directory")
    options, inputs = parser.parse_args()

    results_dir = options.results_dir
    if results_dir is None:
        sys.exit("Please give directory to your results")

    output_dir = options.output_dir
    if output_dir is None:
        sys.exit("Please give a output directory")

    analyze = Analyzer(results_dir, output_dir)
    acked = analyze.process_order_acked()
    entry = analyze.process_order_entry()
    analyze.process_round_trip_to_disk_and_graph(entry, acked)
    analyze.move_files()
