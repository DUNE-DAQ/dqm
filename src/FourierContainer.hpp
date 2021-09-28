/**
 * @file FourierContainer.hpp Implementation of a container of Fourier objects
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_FOURIERCONTAINER_HPP_
#define DQM_SRC_FOURIERCONTAINER_HPP_

// DQM
#include "AnalysisModule.hpp"
#include "Decoder.hpp"
#include "Exporter.hpp"
#include "ChannelMapper.hpp"
#include "dqm/Fourier.hpp"
#include "dqm/Types.hpp"

#include "dataformats/TriggerRecord.hpp"

#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

namespace dunedaq::dqm {

class FourierContainer : public AnalysisModule
{
  std::string m_name;
  std::vector<ChannelInfo> chanvec;
  std::vector<Fourier> fouriervec;
  bool m_run_mark;
  size_t m_size;

public:
  FourierContainer(std::string name, int size, double inc, int npoints);

  void run(dunedaq::dataformats::TriggerRecord& tr, RunningMode mode = RunningMode::kNormal, std::string kafka_address="");
  void transmit(std::string &kafka_address, const std::string& topicname, int run_num, time_t timestamp);
  // void clean();
  // void save_and_clean(uint64_t timestamp); // NOLINT(build/unsigned)
  bool is_running();

};

  FourierContainer::FourierContainer(std::string name, int sizedouble, double inc, int npoints)
  : m_name(name)
  , m_run_mark(false)
  , m_size(npoints)
{
  for (size_t i = 0; i < m_size; ++i)
  {
    fouriervec.emplace_back(Fourier(inc, npoints));
    ChannelInfo chaninf;
    chaninf.APA   = 9999;
    chaninf.Plane = 9999;
    chaninf.Wire  = 9999;
    chaninf.GeoID       = "NULL";
    chaninf.Application = "NULL";
    chaninf.Partition   = "NULL";
    chanvec.emplace_back(chaninf);
  }
}

void
FourierContainer::run(dunedaq::dataformats::TriggerRecord& tr, RunningMode, std::string kafka_address)
{
  m_run_mark = true;
  dunedaq::dqm::Decoder dec;
  auto wibframes = dec.decode(tr);
  // std::uint64_t timestamp = 0; // NOLINT(build/unsigned)
  
  //Get channel map
  std::unique_ptr<swtpg::PdspChannelMapService> channelMap;
  const char* readout_share_cstr = getenv("READOUT_SHARE");
  std::string readout_share(readout_share_cstr);
  std::string channel_map_rce = readout_share +  "/config/protoDUNETPCChannelMap_RCE_v4.txt";
  std::string channel_map_felix = readout_share + "/config/protoDUNETPCChannelMap_FELIX_v4.txt";

  //Populate Fourier vector with time series (+ associated channel info)
  for (auto fr : wibframes) {
    for (size_t ich = 0; ich < m_size; ++ich)
    {
      //Fill time series
      fouriervec[ich].fill(fr->get_channel(ich));
   
      //Fill channel info
      int offline = getOfflineChannel(*channelMap, fr, ich);
      ChannelInfo chaninf;
      chaninf.APA = channelMap->APAFromOfflineChannel(offline);
      chaninf.Plane = channelMap->PlaneFromOfflineChannel(offline);
      chaninf.Wire = LocalWireNumber(*channelMap, offline);
      chanvec[ich] = chaninf;
    }
  }

  //Perform fast Fourier transforms
  for (size_t ich = 0; ich < m_size; ++ich)
  {
    CArray fft = fouriervec[ich].compute_fourier();

    for (size_t i = 0; i < fft.size(); ++i)
    {
      fouriervec[ich].m_data[i] = std::abs(fft[i]);
    }
  }

  //Transmit via kafka
  transmit(kafka_address, "testdunedqm", tr.get_header_ref().get_run_number(), tr.get_header_ref().get_trigger_timestamp());

  m_run_mark = false;

  // clean();
}

bool
FourierContainer::is_running()
{
  return m_run_mark;
}

void
FourierContainer::transmit(std::string& kafka_address, const std::string& topicname, int run_num, time_t timestamp)
{
  std::stringstream csv_output;
  std::string datasource = "TESTSOURCE";
  std::string dataname = this->m_name;
  std::string axislabel = "TESTLABEL";
  std::stringstream metadata;
  //METADATA NEEDS ENABLING
  //metadata << fouriervec[0].m_inc_size << " " << fouriervec[0].m_start << " " << fouriervec[0].m_end << " " << fouriervec[0].m_freq_max;

  int subrun = 0;
  int event = 0;

  // Construct CSV output
  csv_output << datasource << ";" << dataname << ";" << run_num << ";" << subrun << ";" << event << ";" << timestamp
             << ";" << metadata.str() << ";";
  csv_output << axislabel << "\n";
  for (int ich = 0; ich < m_size; ++ich) {
    csv_output << "APA_" << chanvec[ich].APA << " " << "Plane_" << chanvec[ich].Plane << " " << "Wire_" << chanvec[ich].Wire << " " << "GeoID_" << chanvec[ich].GeoID << " " << "Application_" << chanvec[ich].Application << " " << "Partition_" << chanvec[ich].Partition << "\n";
    for (int i = 0; i < fouriervec[ich].m_data.size() / 2; i++) csv_output << fouriervec[ich].m_data.at(i) << " ";
    csv_output << "\n";
  }

  // Transmit
  KafkaExport(kafka_address, csv_output.str(), topicname);

  // clean();
}

// void
// FourierContainer::clean()
// {
//   for (int ich = 0; ich < m_size; ++ich) {
//     fouriervec[ich].clean();
//   }
// }

// void
// FourierContainer::save_and_clean(uint64_t timestamp) // NOLINT(build/unsigned)
// {
//   std::ofstream file;
//   file.open("Hist/" + m_name + "-" + std::to_string(filename_index) + ".txt", std::ios_base::app);
//   file << timestamp << std::endl;

//   for (int ich = 0; ich < m_size; ++ich) {
//     file << fouriervec[ich].m_sum << " ";
//   }
//   file << std::endl;
//   file.close();

//   clean();
// }

} // namespace dunedaq::dqm

#endif // DQM_SRC_FOURIERCONTAINER_HPP_
