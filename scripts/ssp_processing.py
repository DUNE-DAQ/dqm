import math
import numpy as np
##import pandas as pd
##import seaborn as sns
import matplotlib.pyplot as plt
from scipy.fftpack import rfft, rfftfreq

from kafka import KafkaProducer

import sys

from hdf5libs import HDF5RawDataFile
import daqdataformats
import detdataformats.ssp
from rawdatautils import *
import h5py

server = "monkafka.cern.ch:30092"
producer = KafkaProducer(bootstrap_servers=server, max_request_size=101626282)

# Parameters
#filname = "/eos/experiment/neutplatform/protodune/rawdata/np04/vd-protodune-pds/raw/2021/detector/test/None/00/01/21/65/np02_pds_run012165_0000_20211126T181018.hdf5" 
filename = "/eos/user/m/mman/np02_pds_run012450_0000_20220225T100020.hdf5.copied" #np02_arapucas_run011966_0000_20211102T141910.hdf5"
#num_events = 1
save_path = "/afs/cern.ch/user/m/mman/dunedaq-v2.9.0/ssp_test_out/"


def EventHeaderDecoder(filename):
    # initialize variable lists
    module_id = []
    channel_id = []
    ext_timestamp = []
    peaksum = []
    peaktime = []
    prerise = []
    intsum = []
    baseline = []
    baseline_sum = []

    # Extract event_header info
    ssp_file = HDF5RawDataFile(filename)

    for dpath in ssp_file.get_all_fragment_dataset_paths():
        frag = ssp_file.get_frag(dpath)
        if frag.get_element_id().system_type!=daqdataformats.GeoID.SystemType.kPDS:
            print("WARNING: fragment is not PDS type")
            break

        ## Parse SSP frag
        event_header = detdataformats.ssp.EventHeader(frag.get_data())
        ts = (event_header.timestamp[3] << 48) + (event_header.timestamp[2] << 32) + (event_header.timestamp[1] << 16) + (event_header.timestamp[0] << 0)
        ext_timestamp.append(ts)
        module_id.append((event_header.group2 & 0xFFF0) >> 4)
        channel_id.append((event_header.group2 & 0x000F) >> 0)
        peaktime.append((event_header.group3 & 0xFF00) >> 8)
        prerise.append(((event_header.group4 & 0x00FF) << 16) + event_header.preriseLow)
        intsum.append((event_header.intSumHigh << 8) + ((event_header.group4 & 0xFF00) >> 8))
        baseline.append(event_header.baseline)
        baseline_sum.append(((event_header.group4 & 0x00FF) << 16) + event_header.preriseLow)
    
    return (module_id, channel_id, ext_timestamp, peaksum, peaktime, prerise, intsum, baseline, baseline_sum)

(module_id, channel_id, ext_timestamp, peaksum, peaktime, prerise, intsum, baseline, baseline_sum) = EventHeaderDecoder(filename)


# Extract waveforms
ssp_data = SSPDecoder(filename)
ssp_frames = ssp_data.ssp_frames
#print(type(ssp_frames[0]), ssp_frames[0])

if (len(module_id) != len(ssp_frames)):
    print("Fix Me!")

fragment_count = len(module_id)
#print(fragment_count)
n_frags_saved = 28

## Generate plots

## ADC waveform plot 
fig = plt.figure(figsize=(35,15))
ax = fig.add_subplot()

for i in range(n_frags_saved):
    plt.scatter(np.arange(len(ssp_frames[i])), ssp_frames[i], s=40, label="Module_Channel_ID "+str(module_id[i])+"_"+str(channel_id[i]))

plt.ylabel("ADC value")
plt.xlabel('Time ticks')
plt.title("SSP")
plt.grid()
plt.legend()
#plt.legend(loc='upper center', bbox_to_anchor=(0.5, -0.20))

plt.savefig(save_path+"ADC_time.png")
plt.clf()

## Kafka Broadcasting
topic = "testdunedqm"
datasource = 'mman_ssp_test'
dataname = 'ssp_display'
run_num = "012450"
subrun = "0"
event = "0"
timestamp = str(ext_timestamp[0])
metadata = 'metadata'
partition = ''
app_name = ''
plane = '0'

times = np.arange(len(ssp_frames[0]))*6.66 # nanoseconds
#message = datasource+dataname+run_num+subrun+event+timestamp+metadata+partition+app_name+"0;"+plane
message = f'{datasource};{dataname};{run_num};{subrun};{event};{timestamp};{metadata};{partition};{app_name};0;{plane};'
#print(message)
adcmessage = np.array2string(np.array(ssp_frames[0]), max_line_width=np.inf, precision=2, threshold=np.inf, formatter={'float_kind':lambda x: "%.2f" % x}) + ' \n'
timemessage = np.array2string(times, max_line_width=np.inf, precision=2, threshold=np.inf, formatter={'float_kind':lambda x: "%.2f" % x}) + ' \n'

#for i in range(len(times)):
    #val = np.array2string(np.array(ssp_frames[0]), max_line_width=np.inf, precision=2, threshold=np.inf) 
    #message += f'{times[i]}\n{val} \n'

#test message
test_message = message + adcmessage + timemessage

print("Sending raw waveform")
producer.send(topic, bytes(test_message, 'utf-8'))

## FFT plot
fig = plt.figure(figsize=(35,15))
ax = fig.add_subplot()

for i in range(n_frags_saved):
    ## sanity check
    ##if len(ssp_frames[i])<480:
    ##    print(ssp_frames[i])
    ##    continue
    fft = np.abs(rfft(ssp_frames[i]))
    freq = rfftfreq(482)
    plt.plot(freq, fft, label="Module_Channel_ID "+str(module_id[i])+"_"+str(channel_id[i]))

plt.ylabel("FFT")
plt.xlabel("Frequency")
plt.legend()

plt.savefig(save_path+"FFT.png")

## Integrated sum ADC values
#if num_events==1:
#    fig = plt.figure(figsize=(35,15))

#    pivot_data = df.pivot(index="module_id", columns="channel_id", values="intsum")
#    ax = sns.heatmap(pivot_data)

#    plt.title("Integrated sum ADC values")
#    plt.ylabel("module")
#    plt.xlabel("channel")
#    plt.legend()
#
#    plt.savefig(save_path+"Intsum.png")
#else:
#    print("intsum only gets plotted when num_events is 1 for now")


