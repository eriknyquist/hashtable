# Processes the output from perf_test2.c and makes a pretty graph

import os
import sys

import matplotlib
import matplotlib.pyplot as plt


def main():
    if len(sys.argv) < 2:
        print("Usage: %s <output_txt_file>" % sys.argv[0])

    entry_counts = []
    load_factors = []
    insert_times = []
    retrieve_times = []
    badkey_times = []

    with open(sys.argv[1], 'r') as fh:
        for line in fh.readlines():
            fields = line.split(']')[1].split()
            if len(fields) != 5:
                raise ValueError("Malformed output file")

            entry_count = int(fields[0].split("=")[1].rstrip(','))
            load_factor = float(fields[1].split("=")[1].rstrip(','))
            insert_ns = int(fields[2].split("=")[1].rstrip(','))
            retrieve_ns = int(fields[3].split("=")[1].rstrip(','))
            badkey_ns = int(fields[4].split("=")[1].rstrip(','))

            entry_counts.append(entry_count)
            load_factors.append(load_factor)
            insert_times.append(insert_ns)
            retrieve_times.append(retrieve_ns)
            badkey_times.append(badkey_ns)

    fig, ax1 = plt.subplots()
    ax2 = ax1.twinx()

    ax1.set_title("28MB hashtable stress test", weight='bold')
    ax1.get_xaxis().set_major_formatter(matplotlib.ticker.FuncFormatter(lambda x, p: format(int(x), ',')))
    ax2.get_yaxis().set_major_formatter(matplotlib.ticker.FormatStrFormatter('%.2f'))

    ax1.set_xlabel("Number of entries in table", weight='bold')
    ax1.set_ylabel("Nanoseconds", weight='bold')
    ax2.set_ylabel("Load factor", color='r', weight='bold')

    ax2.plot(entry_counts, load_factors, color='r', linestyle='dotted', label="Load factor")

    ax1.plot(entry_counts, retrieve_times, color='limegreen', label="Average item retrieval time (nanoseconds)")
    ax1.plot(entry_counts, badkey_times, color='orange', label="Average time to check for non-existent key (nanoseconds)")
    ax1.plot(entry_counts, insert_times, color='darkviolet', label="Average item insertion time (nanoseconds)")
    ax2.legend(bbox_to_anchor=(1,1), loc='upper right', ncol=1)
    ax1.legend(bbox_to_anchor=(0,1), loc='upper left', ncol=1)
    plt.show()

if __name__ == "__main__":
    main()
