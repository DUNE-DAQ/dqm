cmake_minimum_required(VERSION 3.12)
project(dqm VERSION 1.0.0)

find_package(daq-cmake REQUIRED)

daq_setup_environment()

# Here we put what packages we need

find_package(appfwk REQUIRED)     
find_package(opmonlib REQUIRED)
find_package(readout REQUIRED)
find_package(dataformats REQUIRED)
find_package(timinglibs REQUIRED)
find_package(RdKafka REQUIRED)
find_package(ers REQUIRED)
find_package(HighFive REQUIRED)


daq_codegen(*.jsonnet TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen(*info.jsonnet DEP_PKGS opmonlib TEMPLATES opmonlib/InfoStructs.hpp.j2 opmonlib/InfoNljs.hpp.j2 )

daq_add_application(trigger_transform trigger_transform.cxx LINK_LIBRARIES appfwk::appfwk readout::readout dataformats::dataformats HighFive RdKafka::rdkafka++ ers::ers timinglibs::timinglibs)
daq_add_application(trigger_adc_frequency trigger_adc_frequency.cxx LINK_LIBRARIES appfwk::appfwk readout::readout dataformats::dataformats HighFive RdKafka::rdkafka++ ers::ers timinglibs::timinglibs)


# We put here the name of our class, and the libraries we want to link with

daq_add_plugin(DQMProcessor duneDAQModule LINK_LIBRARIES appfwk::appfwk dataformats::dataformats readout::readout timinglibs::timinglibs RdKafka::rdkafka RdKafka::rdkafka++)

# Unit tests

daq_add_unit_test(Fourier_test LINK_LIBRARIES dataformats::dataformats)

daq_install()
