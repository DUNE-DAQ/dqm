cmake_minimum_required(VERSION 3.12)
project(dqm VERSION 1.1.6)

find_package(daq-cmake REQUIRED)

daq_setup_environment()

# Here we put what packages we need

find_package(appfwk REQUIRED)     
find_package(opmonlib REQUIRED)
find_package(readout REQUIRED)
find_package(daqdataformats REQUIRED)
find_package(detdataformats REQUIRED)
find_package(detchannelmaps REQUIRED)
find_package(timinglibs REQUIRED)
find_package(RdKafka REQUIRED)
find_package(ers REQUIRED)
find_package(HighFive REQUIRED)
FIND_PACKAGE(Boost 1.70.0 COMPONENTS program_options REQUIRED)

daq_codegen(*.jsonnet TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen(*info.jsonnet DEP_PKGS opmonlib TEMPLATES opmonlib/InfoStructs.hpp.j2 opmonlib/InfoNljs.hpp.j2 )

# daq_add_application(trigger_transform trigger_transform.cxx LINK_LIBRARIES appfwk::appfwk readout::readout daqdataformats::daqdataformats detdataformats::detdataformats detchannelmaps::detchannelmaps HighFive RdKafka::rdkafka++ timinglibs::timinglibs ${Boost_PROGRAM_OPTIONS_LIBRARY})

# We put here the name of our class, and the libraries we want to link with

daq_add_plugin(DQMProcessor duneDAQModule LINK_LIBRARIES appfwk::appfwk daqdataformats::daqdataformats detdataformats::detdataformats detchannelmaps::detchannelmaps readout::readout timinglibs::timinglibs RdKafka::rdkafka RdKafka::rdkafka++)
# daq_add_plugin(HDF5Reader duneDAQModule LINK_LIBRARIES appfwk::appfwk daqdataformats::daqdataformats detdataformats::detdataformats detchannelmaps::detchannelmaps timinglibs::timinglibs readout::readout HighFive)

# Unit tests

daq_add_unit_test(Fourier_test LINK_LIBRARIES daqdataformats::daqdataformats detdataformats::detdataformats detchannelmaps::detchannelmaps readout::readout RdKafka::rdkafka++)

daq_install()
