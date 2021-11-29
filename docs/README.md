# Data Quality Monitoring
Software and tools for data monitoring. This module processes data coming from
readout and sends it output to a kafka broker. Then, the data can be displayed and analyzed. Related repos are
[dqmanalysis](https://github.com/DUNE-DAQ/dqmanalysis) and [dqmplatform](https://github.com/DUNE-DAQ/dqmplatform)

This module does *not* include displaying of data.

## Building

How to clone and build DUNE DAQ packages, including dqm, is covered in [the daq-buildtools instructions](https://dune-daq-sw.readthedocs.io/en/latest/packages/daq-buildtools/).

## How to run

* Standalone configuration

To generate the standalone configuration, run

    python sourcecode/dqm/python/dqm/fake_app_confgen.py

This will create a JSON file that can be used to run the `daq_application`

    daq_application -c dqm.json -n dqm

The standalone configuration is as minimal as it can be. It runs readout,
obtaining the data from a file called `frames.bin` (the configuration assumes it
is located in the same directory that you are running from), and sends the
output of the algorithms to a kafka broker.

* Nanorc configuration

The nanorc configuration can be generated with

    python -m minidaqapp.nanorc.mdapp_multiru_gen --enable-dqm nanorc-dqm

And run with

    nanorc nanorc-dqm

## Supported data streams

* Raw display: A contiguous set in time of the ADC in a number of WIB frames will be displayed.

  Use `--dqm-rawdisplay-params N L` where N is how many seconds there are
  between running the raw display algorithm, and L is the number of frames that
  this algorithm will run on.
* Mean and RMS plot: The mean and RMS of the ADC for each channel over a number
  of frames

  Use `--dqm-meanrms-params N L` where N and L have the same meaning as above.
* Fourier transform: The fourier transform of the ADC for each channel over a
  number of frames

  Use `--dqm-fourier-params N L` where N L have the same meaning as above.

## Channel map
To use the horizontal drift channel map (default) with nanorc use `--dqm-cmap HD`,
to use the vertical drift channel map use `--dqm-cmap VD`.
