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
#include "ChannelMap.hpp"
#include "Constants.hpp"
#include "dqm/Hist.hpp"
#include "dqm/DQMIssues.hpp"

#include "daqdataformats/TriggerRecord.hpp"

#include <string>
#include <vector>
#include <cstdlib>

namespace dunedaq::dqm {


class HistContainer : public AnalysisModule
{
  std::string m_name;
  std::vector<Hist> histvec;
  int m_size;
  std::map<int, std::string> m_to_send;
  std::map<int, int> m_index;
  bool m_only_mean_rms = false;

public:
  HistContainer(std::string name, int nhist, int steps, double low, double high, bool only_mean=false);
  HistContainer(std::string name, int nhist, std::vector<int>& link_idx, int steps, double low, double high, bool only_mean);

  void run(std::unique_ptr<daqdataformats::TriggerRecord> record, std::unique_ptr<ChannelMap> &map, std::string kafka_address="");
  void transmit(std::string &kafka_address, std::unique_ptr<ChannelMap> &map, const std::string& topicname, int run_num, time_t timestamp);
  void transmit_mean_and_rms(std::string &kafka_address, std::unique_ptr<ChannelMap> &map, const std::string& topicname, int run_num, time_t timestamp);
  void clean();
  void append_to_string(std::uint64_t timestamp, std::unique_ptr<ChannelMap> &map);
  void fill(int ch, double value);
  void fill(int ch, int link, double value);
  int get_local_index(int ch, int link);

};

HistContainer::HistContainer(std::string name, int nhist, int steps, double low, double high, bool only_mean)
  : m_name(name)
  , m_size(nhist)
  , m_only_mean_rms(only_mean)
{
  for (int i = 0; i < m_size; ++i)
    histvec.emplace_back(Hist(steps, low, high));
}

HistContainer::HistContainer(std::string name, int nhist, std::vector<int>& link_idx, int steps, double low, double high, bool only_mean)
  : m_name(name)
  , m_size(nhist)
  , m_only_mean_rms(only_mean)
{
  for (int i = 0; i < m_size; ++i) {
    histvec.emplace_back(Hist(steps, low, high));
  }
  int channels = 0;
  for (size_t i = 0; i < link_idx.size(); ++i) {
    m_index[link_idx[i]] = channels;
    channels += CHANNELS_PER_LINK;
  }
}

void
HistContainer::run(std::unique_ptr<daqdataformats::TriggerRecord> record, std::unique_ptr<ChannelMap> &map, std::string kafka_address)
{
  m_run_mark.store(true);
  dunedaq::dqm::Decoder dec;
  auto wibframes = dec.decode(*record);

  if (wibframes.size() == 0) {
    // throw issue
    m_run_mark.store(false);
    TLOG() << "Found no frames";
    return;
  }

  // Get all the keys
  std::vector<int> keys;
  for (auto& [key, value] : wibframes) {
    keys.push_back(key);
  }

  uint64_t min_timestamp = 0;
  // We run over all links until we find one that has a non-empty vector of frames
  for (auto& key : keys) {
    if (!wibframes[key].empty()) {
      min_timestamp = wibframes[key].front()->get_wib_header()->get_timestamp();
      break;
    }
  }
  uint64_t timestamp = 0;

  // Check that all the wibframes vectors have the same size, if not, something
  // bad has happened, for now don't do anything
  auto size = wibframes.begin()->second.size();
  for (auto& vec : wibframes) {
    if (vec.second.size() != size) {
      ers::error(InvalidData(ERS_HERE, "the size of the vector of frames is different for each link"));
      m_run_mark.store(false);
      return;
    }
  }

  // Main loop
  // If only the mean and rms are to be sent all frames are processed
  // and at the end the result is transmitted
  // If it's in the raw display mode then the result is saved for
  // every frame and sent at the end

  // Fill for every frame, outer loop so it is done frame by frame
  // This is needed for sending frame by frame
  // The order does not matter for the mean and RMS
  for (size_t ifr = 0; ifr < wibframes[keys[0]].size(); ++ifr) {
    // Fill for every link
    for (size_t ikey = 0; ikey < keys.size(); ++ikey) {
      auto fr = wibframes[keys[ikey]][ifr];

      // Timestamps are too big for them to be displayed nicely, subtract the minimum timestamp
      timestamp = fr->get_wib_header()->get_timestamp() - min_timestamp;

      for (int ich = 0; ich < CHANNELS_PER_LINK; ++ich) {
        fill(ich, keys[ikey], fr->get_channel(ich));
      }
    }
    // After we are done with all the links, if needed save the info for later
    if (!m_only_mean_rms) {
      append_to_string(timestamp, map);
      clean();
    }
  }
  if (m_only_mean_rms) {
    transmit_mean_and_rms(kafka_address, map, "testdunedqm", record->get_header_ref().get_run_number(), record->get_header_ref().get_trigger_timestamp());
  }
  else {
    transmit(kafka_address, map, "testdunedqm", record->get_header_ref().get_run_number(), record->get_header_ref().get_trigger_timestamp());
  }
  clean();

  m_run_mark.store(false);

}

void
HistContainer::append_to_string(std::uint64_t timestamp, std::unique_ptr<ChannelMap> &cmap)
{
  auto channel_order = cmap->get_map();
  for (auto& [plane, map] : channel_order) {
    m_to_send[plane] += std::to_string(timestamp) + "\n";
    for (auto& [offch, pair] : map) {
      int link = pair.first;
      int ch = pair.second;
      m_to_send[plane] += std::to_string(static_cast<int>(histvec[get_local_index(ch, link)].m_sum)) + " ";
    }
    m_to_send[plane] += "\n";
  }
}

void
HistContainer::transmit(std::string& kafka_address, std::unique_ptr<ChannelMap> &cmap, const std::string& topicname, int run_num, time_t timestamp)
{
  // Placeholders
  std::string dataname = m_name;
  std::string metadata = "";
  int subrun = 0;
  int event = 0;
  std::string partition = getenv("DUNEDAQ_PARTITION");
  std::string app_name = getenv("DUNEDAQ_APPLICATION_NAME");
  std::string datasource = partition + "_" + app_name;
  // One message is sent for every plane
  auto channel_order = cmap->get_map();

  for (auto& [key, value] : channel_order) {
    std::stringstream output;
    output << datasource << ";" << dataname << ";" << run_num << ";" << subrun
           << ";" << event << ";" << timestamp << ";" << metadata << ";"
           << partition << ";" << app_name << ";" << 0 << ";" << key << ";";
    for (auto& [offch, ch] : value) {
      output << offch << " ";
    }
    output << "\n";
    output << m_to_send[key];
    TLOG_DEBUG(5) << "Size of the message in bytes: " << output.str().size();
    KafkaExport(kafka_address, output.str(), topicname);
  }
  m_to_send.clear();

}

void
HistContainer::transmit_mean_and_rms(std::string& kafka_address, std::unique_ptr<ChannelMap> &cmap, const std::string& topicname, int run_num, time_t timestamp)
{
  // Placeholders
  std::string dataname = m_name;
  std::string metadata = "";
  int subrun = 0;
  int event = 0;
  std::string partition = getenv("DUNEDAQ_PARTITION");
  std::string app_name = getenv("DUNEDAQ_APPLICATION_NAME");
  std::string datasource = partition + "_" + app_name;

  // TLOG() << "DATASOURCE " << datasource;

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
    output << "Mean\n";
    for (auto& [offch, pair] : map) {
      int link = pair.first;
      int ch = pair.second;
      output << histvec[get_local_index(ch, link)].mean() << " ";
    }
    output << "\n";
    output << "RMS\n";
    for (auto& [offch, pair] : map) {
      int link = pair.first;
      int ch = pair.second;
      output << histvec[get_local_index(ch, link)].std() << " ";
    }
    output << "\n";
    TLOG_DEBUG(5) << "Size of the message in bytes: " << output.str().size();
    KafkaExport(kafka_address, output.str(), topicname);
  }

}

void
HistContainer::clean()
{
  for (int ich = 0; ich < m_size; ++ich) {
    histvec[ich].clean();
  }
}

void
HistContainer::fill(int ch, double value)
{
  histvec[ch].fill(value);
}

void
HistContainer::fill(int ch, int link, double value)
{
  histvec[ch + m_index[link]].fill(value);
}

int
HistContainer::get_local_index(int ch, int link)
{
  return ch + m_index[link];
}


} // namespace dunedaq::dqm

#endif // DQM_SRC_HISTCONTAINER_HPP_
