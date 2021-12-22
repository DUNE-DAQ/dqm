cmake_minimum_required(VERSION 3.12)
project(dqm VERSION 1.2.0)

find_package(daq-cmake REQUIRED)

daq_setup_environment()

# Here we put what packages we need

find_package(appfwk REQUIRED)     
find_package(opmonlib REQUIRED)
find_package(readoutlibs REQUIRED)
find_package(daqdataformats REQUIRED)
find_package(detdataformats REQUIRED)
find_package(detchannelmaps REQUIRED)
find_package(timinglibs REQUIRED)
find_package(RdKafka REQUIRED)
find_package(ers REQUIRED)
find_package(HighFive REQUIRED)
find_package(Boost COMPONENTS unit_test_framework program_options REQUIRED)

daq_codegen(*.jsonnet TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen(*info.jsonnet DEP_PKGS opmonlib TEMPLATES opmonlib/InfoStructs.hpp.j2 opmonlib/InfoNljs.hpp.j2 )

# We put here the name of our class, and the libraries we want to link with

daq_add_plugin(DQMProcessor duneDAQModule LINK_LIBRARIES appfwk::appfwk daqdataformats::daqdataformats detdataformats::detdataformats detchannelmaps::detchannelmaps readoutlibs::readoutlibs timinglibs::timinglibs RdKafka::rdkafka RdKafka::rdkafka++)
# daq_add_plugin(HDF5Reader duneDAQModule LINK_LIBRARIES appfwk::appfwk daqdataformats::daqdataformats detdataformats::detdataformats detchannelmaps::detchannelmaps timinglibs::timinglibs readout::readout HighFive)

# Unit tests

daq_add_unit_test(Fourier_test LINK_LIBRARIES daqdataformats::daqdataformats detdataformats::detdataformats detchannelmaps::detchannelmaps readoutlibs::readoutlibs RdKafka::rdkafka++)

daq_install()
