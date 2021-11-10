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
#include "ChannelMap.hpp"
#include "Constants.hpp"
#include "Decoder.hpp"
#include "Exporter.hpp"
#include "dqm/Fourier.hpp"

#include "daqdataformats/TriggerRecord.hpp"

#include <fstream>
#include <ostream>
#include <string>
#include <vector>
#include <cstdlib>

namespace dunedaq::dqm {

class FourierContainer : public AnalysisModule
{
  std::string m_name;
  std::vector<Fourier> fouriervec;
  size_t m_size;
  int m_npoints;
  std::map<int, int> m_index;

public:
  FourierContainer(std::string name, int size, double inc, int npoints);
  FourierContainer(std::string name, int size, std::vector<int>& link_idx, double inc, int npoints);

  void run(std::unique_ptr<daqdataformats::TriggerRecord> record, std::unique_ptr<ChannelMap> &map, std::string kafka_address="");
  void transmit(std::string &kafka_address, std::unique_ptr<ChannelMap> &cmap, const std::string& topicname, int run_num, time_t timestamp);
  void clean();
  void fill(int ch, double value);
  void fill(int ch, int link, double value);
  int get_local_index(int ch, int link);

};

FourierContainer::FourierContainer(std::string name, int size, double inc, int npoints)
  : m_name(name),
    m_size(size),
    m_npoints(npoints)
{
  for (size_t i = 0; i < m_size; ++i) {
    fouriervec.emplace_back(Fourier(inc, npoints));
  }
}

FourierContainer::FourierContainer(std::string name, int size, std::vector<int>& link_idx, double inc, int npoints)
  : m_name(name),
    m_size(size),
    m_npoints(npoints)
{
  for (size_t i = 0; i < m_size; ++i) {
    fouriervec.emplace_back(Fourier(inc, npoints));
  }
  int channels = 0;
  for (size_t i = 0; i < link_idx.size(); ++i) {
    m_index[link_idx[i]] = channels;
    channels += CHANNELS_PER_LINK;
  }
}
void
FourierContainer::run(std::unique_ptr<daqdataformats::TriggerRecord> record, std::unique_ptr<ChannelMap> &map, std::string kafka_address)
{
  m_run_mark.store(true);
  dunedaq::dqm::Decoder dec;
  auto wibframes = dec.decode(*record);
  // std::uint64_t timestamp = 0; // NOLINT(build/unsigned)

  for (auto& [key, value] : wibframes) {
    for (auto& fr : value) {
      for (size_t ich = 0; ich < CHANNELS_PER_LINK; ++ich) {
        fill(ich, key, fr->get_channel(ich));
      }
    }
  }

  for (size_t ich = 0; ich < m_size; ++ich) {
    fouriervec[ich].compute_fourier_normalized();
  }

  transmit(kafka_address, map, "testdunedqm", record->get_header_ref().get_run_number(), record->get_header_ref().get_trigger_timestamp());

  m_run_mark.store(false);
}

void
FourierContainer::transmit(std::string &kafka_address, std::unique_ptr<ChannelMap> &cmap, const std::string& topicname, int run_num, time_t timestamp)
{
  // Placeholders
  std::string dataname = m_name;
  std::string metadata = "";
  int subrun = 0;
  int event = 0;
  std::string partition = getenv("DUNEDAQ_PARTITION");
  std::string app_name = getenv("DUNEDAQ_APPLICATION_NAME");
  std::string datasource = partition + "_" + app_name;


  auto freq = fouriervec[0].get_frequencies();
  // One message is sent for every plane
  auto channel_order = cmap->get_map();
  for (auto& [plane, map] : channel_order) {
    std::stringstream output;
    output << datasource << ";" << dataname << ";" << run_num << ";" << subrun
           << ";" << event << ";" << timestamp << ";" << metadata << ";"
           << partition << ";" << app_name << ";" << 0 << ";" << plane << ";";
    for (auto& [offch, pair] : map) {
      output << offch << " ";
    }
    output << "\n";
    for (size_t i=0; i < freq.size(); ++i) {
      output << freq[i] << "\n";
      for (auto& [offch, pair] : map) {
        int link = pair.first;
        int ch = pair.second;
        output << fouriervec[get_local_index(ch, link)].get_transform(i) << " ";
      }
      output << "\n";
    }
    TLOG_DEBUG(5) << "Size of the message in bytes: " << output.str().size();
    KafkaExport(kafka_address, output.str(), topicname);
  }

  clean();
}

void
FourierContainer::clean()
{
  for (size_t ich = 0; ich < m_size; ++ich) {
    fouriervec[ich].clean();
  }
}

void
FourierContainer::fill(int ch, double value)
{
  fouriervec[ch].fill(value);
}

void
FourierContainer::fill(int ch, int link, double value)
{
  fouriervec[ch + m_index[link]].fill(value);
}

int
FourierContainer::get_local_index(int ch, int link)
{
  return ch + m_index[link];
}


} // namespace dunedaq::dqm

#endif // DQM_SRC_FOURIERCONTAINER_HPP_
