/**
 * @file HistContainer.hpp Implementation of a container of Hist objects
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_HISTCONTAINER_HPP_
#define DQM_SRC_HISTCONTAINER_HPP_

// DQM
#include "AnalysisModule.hpp"
#include "Decoder.hpp"
#include "Exporter.hpp"
#include "ChannelMapper.hpp"
#include "dqm/Hist.hpp"
#include "dqm/Types.hpp"

#include "dataformats/TriggerRecord.hpp"

#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

namespace dunedaq::dqm {

class HistContainer : public AnalysisModule
{
  std::string m_name;
  std::vector<ChannelInfo> chanvec;
  std::vector<Hist> histvec;
  bool m_run_mark;
  int m_size;

public:
  HistContainer(std::string name, int nhist, int steps, double low, double high);

  void run(dunedaq::dataformats::TriggerRecord& tr, RunningMode mode = RunningMode::kNormal, std::string kafka_address="");
  void transmit(std::string &kafka_address, const std::string& topicname, int run_num, time_t timestamp);
  void clean();
  void save_and_clean(uint64_t timestamp); // NOLINT(build/unsigned)
  bool is_running();

  int frames_run = 0;
  int max_frames = 1;
  int filename_index = 0;
  int current_index = 0;
};

HistContainer::HistContainer(std::string name, int nhist, int steps, double low, double high)
  : m_name(name)
  , m_run_mark(false)
  , m_size(nhist)
{
  for (int i = 0; i < m_size; ++i)
  {
    histvec.emplace_back(Hist(steps, low, high));
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
HistContainer::run(dunedaq::dataformats::TriggerRecord& tr, RunningMode mode, std::string kafka_address)
{
  m_run_mark = true;
  dunedaq::dqm::Decoder dec;
  auto wibframes = dec.decode(tr);
  std::uint64_t timestamp = 0; // NOLINT(build/unsigned)

  //Get channel map
  std::unique_ptr<swtpg::PdspChannelMapService> channelMap;
  const char* readout_share_cstr = getenv("READOUT_SHARE");
  std::string readout_share(readout_share_cstr);
  std::string channel_map_rce = readout_share +  "/config/protoDUNETPCChannelMap_RCE_v4.txt";
  std::string channel_map_felix = readout_share + "/config/protoDUNETPCChannelMap_FELIX_v4.txt";

  for (auto fr : wibframes) {

    for (int ich = 0; ich < m_size; ++ich)
    {
      //Fill histogram
      histvec[ich].fill(fr->get_channel(ich));

      //Fill channel info
      int offline = getOfflineChannel(*channelMap, fr, ich);
      ChannelInfo chaninf;
      chaninf.APA = channelMap->APAFromOfflineChannel(offline);
      chaninf.Plane = channelMap->PlaneFromOfflineChannel(offline);
      chaninf.Wire = LocalWireNumber(*channelMap, offline);
      chanvec[ich] = chaninf;
    }

    // Debug mode - save to a file
    // if (mode != RunningMode::kLocalProcessing) continue;
    if (++frames_run == max_frames) // NOLINT(runtime/increment_decrement)
    {
      timestamp = fr->get_wib_header()->get_timestamp();
      if (mode == RunningMode::kLocalProcessing) {
        save_and_clean(timestamp);
      } else if (mode == RunningMode::kNormal) {
        transmit(kafka_address, "testdunedqm", tr.get_header_ref().get_run_number(), tr.get_header_ref().get_trigger_timestamp());
      }
      frames_run = 0;
      ++current_index;
      if (current_index == 2000) {
        ++filename_index;
        current_index = 0;
      }
    }
  }

  // Transmit via kafka
  // if (mode == RunningMode::kNormal){
  //
  // }

  m_run_mark = false;

  // clean();
}

bool
HistContainer::is_running()
{
  return m_run_mark;
}

void
HistContainer::transmit(std::string& kafka_address, const std::string& topicname, int run_num, time_t timestamp)
{
  std::stringstream csv_output;
  std::string datasource = "TESTSOURCE";
  std::string dataname = this->m_name;
  std::string axislabel = "TESTLABEL";
  std::stringstream metadata;
  metadata << histvec[0].m_steps << " " << histvec[0].m_low << " " << histvec[0].m_high;

  int subrun = 0;
  int event = 0;

  // Construct CSV output
  csv_output << datasource << ";" << dataname << ";" << run_num << ";" << subrun << ";" << event << ";" << timestamp
             << ";" << metadata.str() << ";";
  csv_output << axislabel << "\n";
  for (int ich = 0; ich < m_size; ++ich) {
    csv_output << "APA_" << chanvec[ich].APA << " " << "Plane_" << chanvec[ich].Plane << " " << "Wire_" << chanvec[ich].Wire << " " << "GeoID_" << chanvec[ich].GeoID << " " << "Application_" << chanvec[ich].Application << " " << "Partition_" << chanvec[ich].Partition << "\n";
    csv_output << histvec[ich].m_sum;
    csv_output << "\n";
  }
  
  //csv_output << m_to_send;
  //m_to_send = "";

  // Transmit
  KafkaExport(kafka_address, csv_output.str(), topicname);

  clean();
}

void
HistContainer::clean()
{
  for (int ich = 0; ich < m_size; ++ich) {
    histvec[ich].clean();
  }
}

void
HistContainer::save_and_clean(uint64_t timestamp) // NOLINT(build/unsigned)
{
  // TLOG() << "Saving and cleaning histograms";

  std::ofstream file;
  file.open("Hist/" + m_name + "-" + std::to_string(filename_index) + ".txt", std::ios_base::app);
  file << timestamp << std::endl;

  for (int ich = 0; ich < m_size; ++ich) {
    file << histvec[ich].m_sum << " ";
  }
  file << std::endl;
  file.close();

  clean();
}

} // namespace dunedaq::dqm

#endif // DQM_SRC_HISTCONTAINER_HPP_
