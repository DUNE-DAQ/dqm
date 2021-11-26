import h5py # Reading files
import time # Timing the decoder
import os   # Dealing with folders

import math
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from scipy.fft import rfft, fftfreq

from datetime import datetime
import subprocess
import json
from kafka import KafkaProducer

conf = json.load(open('configuration.json',))
host = conf["FileStorage"]
producer = KafkaProducer(bootstrap_servers=conf["Kafka"]["Servers"], max_request_size=101626282)

import random

def DataOutput(topic, dataSourceName, dataName, dataId, host, recordCount, timeStamp, extensionName, run, subRun, event):
    # storageType = "BSON" 
    
    #Data path summary
    # dataPath = host + "/" + dataSourceName + "/" + dataName + "/" + timeStamp + extensionName
    #Add the path to the database
    # recordId = DbStatements.addDataFile(dataPath, storageType, dataId, run, subRun, event, timeStamp)
    #announces via kafka the new entry
    sendData(topic, bytes(dataId + "," + recordId + "," + dataPath + "," + storageType + "," + dataName  + "," + run + "," + subRun + "," + event + "," + timeStamp, 'utf-8'))
    recordCount = recordCount + 1

def sendData(topic, content):
    future = producer.send(topic, content)
    try:
        record_metadata = future.get(timeout=10)
    except Exception as err:
        print(err)
        pass



topic = "testdunedqm"
datasource = "file_reader"
dataname = 'fft_sums_display'
run_num = '0'
subrun = '0'
event = '0'
timestamp = str(datetime.timestamp(datetime.now()))
metadata = '12345'
partition = ''
app_name = ''
plane = '1'

message = f'{datasource};{dataname};{run_num};{subrun};{event};{timestamp};{metadata};{partition};{app_name};0;{plane};'
freqmessage = '2 3 4 5 \n'
coremessage = 'SummedFFT\n'
numbers = '5 1 3 2 \n'
content = message + freqmessage + coremessage + numbers
#     producer.send(topic, bytes(content, 'utf-8'))

# while True:
#     print('Sending')
#     producer.send(topic, bytes(content, 'utf-8'))
#     time.sleep(5)

# filename = "np02_bde_coldbox_run011918_0001_20211029T122926.hdf5"

dic = {}
dic2 = {}
def channel_map():

    ls = [x.split() for x in open('channel_mapvd.txt').read().split('\n') if x]
    for line in ls:
        dic[(int(line[1]), int(line[2]), int(line[3]))] = int(line[0])
        dic2[int(line[0])] = 'UYZ'.find(line[-1][0])

channel_map()

def get_offline_channel(slot, fiber, chan):
    wc = fiber*2 - 1
    if (chan>127):
      chan -= 128
      wc += 1
    wib, wibconnector, cechan = slot + 1, wc, chan
    return dic[wib, wibconnector, cechan]

def decoder(ary):
    t0 = time.time()
    bits = ary
    # Get the position in the array
    ls = []
    for index in range(256):
        original = index
        pos = 16 # WIBFrameHeader
        block = index // 64
        index %= 64
        pos += 16 * (block+1) + 96 * block # ColdataBlockHeader
        adc = index // 8
        ch = index % 8
        segment_id = adc // 2 * 2 + ch // 4
        pos += 12 * segment_id

        # Final position
        if adc % 2 == 0:
            if ch % 4 == 0:
                ls.append((20, 0, 4, 8))
            elif ch % 4 == 1:
                ls.append((32, 16, 8, 4))
            elif ch % 4 == 2:
                ls.append((68, 48, 4, 8))
            elif ch % 4 == 3:
                ls.append((80, 64, 8, 4))
        elif adc % 2 == 1:
            if ch % 4 == 0:
                ls.append((28, 8, 4, 8))
            elif ch % 4 == 1:
                ls.append((40, 24, 8, 4))
            elif ch % 4 == 2:
                ls.append((76, 56, 4, 8))
            elif ch % 4 == 3:
                ls.append((88, 72, 8, 4))
        pos *= 8
        first = pos + ls[-1][0]
        second = pos + ls[-1][1]
        ls[-1] = (first, second, ls[-1][2], ls[-1][3])

    res = np.zeros((bits.shape[0], 256))
    for i in range(256):
        first, second, size_first, size_second = ls[i]
        if size_second == 4:
            # tmp_second = np.concatenate( (np.repeat(aux, bits.shape[0], axis=0), bits[:, second: second + size_second]), axis=1 )
            tmp_second = np.right_shift(np.packbits(bits[:, second: second + size_second], 1), 4).flatten()
        else:
            tmp_second = np.packbits(bits[:, second: second + size_second])
        if size_first == 4:
            # print('printing shape')
            # print(np.repeat(aux, bits.shape[0], axis=0).shape, bits[:, first: first + size_first].shape)
            # tmp_first = np.concatenate( (np.repeat(aux, bits.shape[0], axis=0), bits[:, first: first + size_first]), axis=1 )
            tmp_first = np.right_shift(np.packbits(bits[:, first: first + size_first], 1), 4).flatten()
        else:
            tmp_first = np.packbits(bits[:, first: first + size_first])

        tmp = tmp_first.astype(np.uint16) * (2**size_second) + tmp_second
        res[:, i] = tmp_first.astype(np.uint16) * (2**size_second) + tmp_second
    print(f'Time for decoding {time.time() - t0:.3f}' )
    return res

def decoder_test():
    """ 
    Test that the decoder is returning the correct values 
    by comparing with a TriggerRecord saved to a binary file
    with known values for each channel
    """
    ary = open("./file.txt", 'rb').read()
    ary = ary.hex()
    a = []
    for i in range(len(ary)):
        nary = ary[i]
        a.append('{0:04b}'.format(int(nary, base=16)))
    ary = np.fromstring(''.join(a), 'u1') - ord('0')
    decoded = decoder(ary)
    for i in range(256):
        assert decoded[i] == i * 8
# decoder_test()

def process_file(filename, folder_name):

    f = h5py.File(filename, "r")

    ls = []
    frame = 8192
    # frame = 10
    keys = f.keys()
    tr = list(keys)[0]
    links = list(f[f'{tr}']['TPC']['CRP004'])
    removed_links = [i for i in range(7) if f'Link0{i}' not in links]
    print('The following links are not present in the file', removed_links)
    for link in links:
        ary = f[f'{tr}']['TPC']['CRP004'][link][80 + 464 * 0: 80 + 464 * frame]
        # ary = ary.reshape((-1, 464))
        # print(ary.shape)
        ary = np.unpackbits(ary.astype(np.uint8)).reshape((-1, 464 * 8))
        tmp = decoder(ary)
        ls.append(tmp)
    df = pd.DataFrame(np.concatenate(ls, axis=1))
    # print(df)

    nls = []
    j = 0
    for slot in [0, 1, 2, 3]:
        for fiber in [1, 2]:
            if j in removed_links:
                j += 1
                continue
            for i in range(256):
                if slot == 3 and fiber == 2:
                    break
                ch = get_offline_channel(slot, fiber, i)
                plane = dic2[ch]
                nls.append(ch)
            j += 1

    nls = np.array(nls)
    indexes = np.argsort(nls)
    df = pd.DataFrame(np.concatenate(ls, axis=1)[:, indexes], columns=nls[indexes])

    print(df.shape)

    # Planes go from 1600 to 1983
    #                1984 to 2623
    #                2624 to 3199

    planes = [1600, 1984, 2624, 3200]

    mi = df.loc[:, :planes[-1]].min()
    ma = df.loc[:, :planes[-1]].max()
    # Raw data displays
    # fig, axls = plt.subplots(1, 3, figsize=(11.16, 2.3))
    # for i in range(3):
    #     ax = axls[i]
    #     cb = ax.imshow(df.loc[:, planes[i]:planes[i+1]],
    #                 origin='lower')
    #     # colorbar = fig.colorbar(cb, ax=ax)
    #     ax.set_xlabel('Channel')
    #     ax.set_ylabel('Frame number')
    # fig.savefig(f'{folder_name}/test_raw_display.png')

    # Raw
    n = 300
    RUNNUM = random.randint(0, 15000)
    METADATA = random.randint(0, 200)
    for i in range(3):
        # ary = df.loc[:250, planes[i]:planes[i+1]]
        ary = df.loc[:n, planes[i]:planes[i+1]]
        # ary -= ary.mean(axis=0)
        # ary = ary.round()
        
        times = np.arange(n) * 25

        topic = "testdunedqm"
        datasource = "file_reader"
        dataname = 'raw_display'
        run_num = RUNNUM
        subrun = '0'
        event = '0'
        timestamp = str(datetime.timestamp(datetime.now()))
        metadata = METADATA
        partition = ''
        app_name = ''

        channels = np.arange(planes[i], planes[i+1])
        plane = i
        message = f'{datasource};{dataname};{run_num};{subrun};{event};{timestamp};{metadata};{partition};{app_name};0;{plane};'
        print(message)
        message += np.array2string(channels, max_line_width=np.inf, precision=2, threshold=np.inf)[1:-1] + ' \n'

        for j in range(len(times)):
            row = np.array2string(ary.iloc[j].values, max_line_width=np.inf, precision=2, threshold=np.inf, formatter={"float_kind":lambda x: "%d" % x})[1:-1]
            message += f'{times[j]}\n{row} \n'
        print('Size of message is ', len(message))
        # input()

        # channelsmessage = np.array2string(channels, max_line_width=np.inf, precision=2, threshold=np.inf)[1:-1] + ' \n'
        # coremessage = 'Mean\n'
        # freqmessage = np.array2string(mean, max_line_width=np.inf, precision=2, threshold=np.inf, formatter={'float_kind':lambda x: "%.2f" % x})[1:-1] + ' \n'
        # core2message = 'RMS\n'
        # freq2message = np.array2string(std, max_line_width=np.inf, precision=2, threshold=np.inf, formatter={'float_kind':lambda x: "%.2f" % x})[1:-1] + ' \n'
        # # numbers = np.array2string(fft[1:-1], max_line_width=np.inf, precision=2, threshold=np.inf)[1:-1] + ' \n'
        # # # print(message, numbers)
        # content = message + channelsmessage + coremessage + freqmessage + core2message + freq2message
        print('Sending raw')
        producer.send(topic, bytes(message, 'utf-8'))


    # Mean and RMS plots
    # fig, axls = plt.subplots(1, 3, figsize=(11.16, 2.3))
    for i in range(3):
        # ax = axls[i]
        # ax.plot(df.loc[:, planes[i]:planes[i+1]].mean(axis=0), 'o', color='C0', label='Mean')
        # ax.plot(df.loc[:, planes[i]:planes[i+1]].std(axis=0), 's', color='C1', label='Std. Dev')
        # ax.legend()
        # twin.legend()
        # ax.set_xlabel('Channel')
        # ax.set_ylabel('Standard Deviation')
        # if i == 1:
        #     ax.text(.5, .9, tr,
        #             transform=ax.transAxes, weight='bold') 

        # fig.savefig(f'{folder_name}/test_mean_rms.png')

        channels = np.arange(planes[i], planes[i+1])

        mean = df.loc[:, planes[i]:planes[i+1]].mean(axis=0).values
        std = df.loc[:, planes[i]:planes[i+1]].std(axis=0).values

        topic = "testdunedqm"
        datasource = "file_reader"
        dataname = 'rmsm_display'
        run_num = '0'
        subrun = '0'
        event = '0'
        timestamp = str(datetime.timestamp(datetime.now()))
        metadata = '12345'
        partition = ''
        app_name = ''

        plane = i
        message = f'{datasource};{dataname};{run_num};{subrun};{event};{timestamp};{metadata};{partition};{app_name};0;{plane};'
        channelsmessage = np.array2string(channels, max_line_width=np.inf, precision=2, threshold=np.inf)[1:-1] + ' \n'
        coremessage = 'Mean\n'
        freqmessage = np.array2string(mean, max_line_width=np.inf, precision=2, threshold=np.inf, formatter={'float_kind':lambda x: "%.2f" % x})[1:-1] + ' \n'
        core2message = 'RMS\n'
        freq2message = np.array2string(std, max_line_width=np.inf, precision=2, threshold=np.inf, formatter={'float_kind':lambda x: "%.2f" % x})[1:-1] + ' \n'
        # numbers = np.array2string(fft[1:-1], max_line_width=np.inf, precision=2, threshold=np.inf)[1:-1] + ' \n'
        # # print(message, numbers)
        content = message + channelsmessage + coremessage + freqmessage + core2message + freq2message
        print('Sending mean/rms')
        producer.send(topic, bytes(content, 'utf-8'))

    # Fourier plot
    for i in range(3):
        # fig, ax = plt.subplots(1, 1, figsize=(3.72, 2.3))
        tmp = df.loc[:, planes[i]:planes[i+1]].sum(axis=1)
        fft = np.abs(rfft(tmp.values))
        freq = fftfreq(frame, 500e-9)

        # ax.plot(freq[1:len(freq)//2], fft[1:-1])
        # ax.set_xlabel('Frequency')
        # ax.set_ylabel('FFT')
    
        # fig.savefig(f'{folder_name}/test_fourier.png')
        print('Plots done')

        topic = "testdunedqm"
        datasource = "file_reader"
        dataname = 'fft_sums_display'
        run_num = '0'
        subrun = '0'
        event = '0'
        timestamp = str(datetime.timestamp(datetime.now()))
        metadata = '12345'
        partition = ''
        app_name = ''

        plane = i
        message = f'{datasource};{dataname};{run_num};{subrun};{event};{timestamp};{metadata};{partition};{app_name};0;{plane};'
        freqmessage = np.array2string(freq[1:len(freq)//2], max_line_width=np.inf, precision=2, threshold=np.inf)[1:-1] + ' \n'
        coremessage = 'SummedFFT\n'
        numbers = np.array2string(fft[1:-1], max_line_width=np.inf, precision=2, threshold=np.inf)[1:-1] + ' \n'
        # print(freqmessage, numbers)
        content = message + freqmessage + coremessage + numbers
        producer.send(topic, bytes(content, 'utf-8'))


while True:
    # Check if there are new files
    # out = subprocess.run("ssh jcarcell@np04-srv-002.cern.ch 'cd /data0 && ls -lah'",
    #                      shell=True, capture_output=True)

    # Assume ls is the list of files
    cdir = os.listdir('.')
    # ls = os.listdir('/data0')
    ls = ['/data0/np02_bde_coldbox_run012142_0005_20211118T101153.hdf5.copied']
    found = None
    possible_files = []
    for name in ls:
        if not name:
            continue
        # name = name.decode('utf-8')
        print(name)
        if 'np02_bde_coldbox' in name and 'json' not in name and name + '.processed' not in cdir and 'writing' not in name:
            possible_files.append(name)
    possible_files.sort()
    print(possible_files)
    # found = possible_files[-1]
    found = '/data0/np02_bde_coldbox_run012159_0015_20211123T060445.hdf5.copied'
    print(f'Found file {found}')

    if found is None:
        time.sleep(30)
        continue

    # Copy the file
    print('Copying the file...')
    # out = subprocess.run(f'rsync -avuP jcarcell@np04-srv-002:/data0/{found} .',
    #                      shell=True)

    # Make a folder to store the results for the file
    folder_name = found[:-5]
    print(f'Creating folder {folder_name}')
    try:
        os.mkdir(folder_name)
    except FileExistsError:
        print('There is already a folder for this file. Results will be overwritten')

    # Process the file
    # process_file('/data0/' + found, folder_name)
    process_file(found, folder_name)
    # Add an empty file to avoid reprocessing
    # open(found + '.processed', 'w').close()

    time.sleep(10)
