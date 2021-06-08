cmake_minimum_required(VERSION 3.12)
project(dqm VERSION 1.0.0)

find_package(daq-cmake REQUIRED)

daq_setup_environment()

daq_codegen(*.jsonnet TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )

# Here we put what packages we need

find_package(appfwk REQUIRED)     
find_package(dataformats REQUIRED)
# find_package(dfmodules REQUIRED)
# find_package(ers REQUIRED)     
# find_package(TRACE REQUIRED)
# find_package(Boost COMPONENTS unit_test_framework program_options REQUIRED)
find_package(RdKafka REQUIRED)

# We put here the name of our class, and the libraries we want to link with

daq_add_library()

# daq_add_plugin(PrintClass duneDAQModule LINK_LIBRARIES appfwk::appfwk dataformats::dataformats dqm readout)
daq_add_plugin(DataReceiver duneDAQModule LINK_LIBRARIES appfwk::appfwk dataformats::dataformats dqm readout RdKafka::rdkafka RdKafka::rdkafka++)

daq_install()
