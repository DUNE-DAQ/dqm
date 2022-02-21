import time
import os

import math
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from scipy.fftpack import rfft, rfftfreq

#from kafka import KafkaProducer

from hdf5libs import *


# Parameters
#filname = "/eos/experiment/neutplatform/protodune/rawdata/np04/vd-protodune-pds/raw/2021/detector/test/None/00/01/21/65/np02_pds_run012165_0000_20211126T181018.hdf5" 
filename = "/eos/user/m/mman/np02_arapucas_run011966_0000_20211102T141910.hdf5"
num_events = 1
save_path = "/afs/cern.ch/user/m/mman/dune_daq/ssp_testing/test_output/"

# Process file
ssp_data = SSPDecoder(filename, num_events)

ssp_frames = ssp_data.ssp_frames

df = pd.DataFrame({"module_id": ssp_data.module_id,
                   "channel_id": ssp_data.channel_id,
                   "intsum": ssp_data.intsum})

fragment_count = len(ssp_data.module_id)

##print(len(module_channel_id))
##print(len(ssp_data.ssp_frames[0]))

## Generate plots

## ADC values over time 
fig = plt.figure(figsize=(35,15))
ax = fig.add_subplot()

for i in range(fragment_count):
    plt.scatter(np.arange(len(ssp_frames[i])), ssp_frames[i], s=40, label="Module_Channel_ID "+str(ssp_data.module_id[i])+"_"+str(ssp_data.channel_id[i]))

plt.ylabel("ADC value")
plt.xlabel('Time ticks')
plt.title("SSP")
plt.grid()
plt.legend()
#plt.legend(loc='upper center', bbox_to_anchor=(0.5, -0.20))

plt.savefig(save_path+"ADC_time.png")
plt.clf()

## FFT ADC values
fig = plt.figure(figsize=(35,15))
ax = fig.add_subplot()

for i in range(fragment_count):
    ## sanity check
    ##if len(ssp_frames[i])<480:
    ##    print(ssp_frames[i])
    ##    continue
    fft = np.abs(rfft(ssp_frames[i]))
    freq = rfftfreq(482)
    plt.plot(freq, fft, label="Module_Channel_ID "+str(ssp_data.module_id[i])+"_"+str(ssp_data.channel_id[i]))

plt.ylabel("FFT")
plt.xlabel("Frequency")
plt.legend()

plt.savefig(save_path+"FFT.png")

## Integrated sum ADC values
fig = plt.figure(figsize=(35,15))

pivot_data = df.pivot(index="module_id", columns="channel_id", values="intsum")
ax = sns.heatmap(pivot_data)

plt.title("Integrated sum ADC values")
plt.ylabel("module")
plt.xlabel("channel")
##plt.legend()

plt.savefig(save_path+"Intsum.png")



