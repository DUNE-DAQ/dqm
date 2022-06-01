cmake_minimum_required(VERSION 3.12)
project(dqm VERSION 1.3.4p1)

find_package(daq-cmake REQUIRED)

daq_setup_environment()

# Here we put what packages we need

find_package(daqdataformats REQUIRED)
find_package(detdataformats REQUIRED)
find_package(detchannelmaps REQUIRED)
find_package(dfmessages REQUIRED)
find_package(RdKafka REQUIRED)
find_package(timinglibs REQUIRED)
find_package(Boost COMPONENTS unit_test_framework program_options REQUIRED)
find_package(fftw REQUIRED)

daq_codegen(*.jsonnet TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen(*info.jsonnet DEP_PKGS opmonlib TEMPLATES opmonlib/InfoStructs.hpp.j2 opmonlib/InfoNljs.hpp.j2 )

set(DQM_DEPENDENCIES
daqdataformats::daqdataformats
detdataformats::detdataformats
detchannelmaps::detchannelmaps
dfmessages::dfmessages
RdKafka::rdkafka
RdKafka::rdkafka++
timinglibs::timinglibs
fftw::fftw3
)

# We put here the name of our class, and the libraries we want to link with

daq_add_plugin(DQMProcessor duneDAQModule LINK_LIBRARIES ${DQM_DEPENDENCIES})
target_include_directories(dqm_DQMProcessor_duneDAQModule PUBLIC $ENV{FFTW_INC})

# Unit tests
daq_add_unit_test(Fourier_test LINK_LIBRARIES ${DQM_DEPENDENCIES})

daq_install()
