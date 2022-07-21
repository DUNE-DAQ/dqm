/**
 * @file ChannelMask.hpp Implementation of a stream of masked channels
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_CHANNELMASK_HPP_
#define DQM_SRC_CHANNELMASK_HPP_

// DQM
#include "AnalysisModule.hpp"
#include "ChannelMap.hpp"
#include "Constants.hpp"
#include "Decoder.hpp"
#include "Exporter.hpp"
#include "dqm/DQMIssues.hpp"

#include "daqdataformats/TriggerRecord.hpp"

#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dunedaq::dqm {

class ChannelMask : public AnalysisModule
{

public:
  ChannelMask(std::string name, std::vector<int>& link_idx);

  std::unique_ptr<daqdataformats::TriggerRecord>
  run(std::unique_ptr<daqdataformats::TriggerRecord> record,
      std::atomic<bool>& run_mark,
      std::shared_ptr<ChannelMap>& map,
      std::string& frontend_type,
      const std::string& kafka_address = "");

  std::unique_ptr<daqdataformats::TriggerRecord>
  run_wib2frame(std::unique_ptr<daqdataformats::TriggerRecord> record,
                std::shared_ptr<ChannelMap>& map,
                const std::string& kafka_address = "");
  void transmit(const std::string& kafka_address,
                std::shared_ptr<ChannelMap>& map,
                const std::string& topicname,
                int run_num,
                time_t timestamp);

  int get_local_index(int ch, int link);

  void clean();
private:
  std::string m_name;
  std::map<int, int> m_index;
  std::vector<bool> mask_vec;

};

ChannelMask::ChannelMask(std::string name, std::vector<int>& link_idx)
  : m_name(name)
{
  int channels = 0;
  for (size_t i = 0; i < link_idx.size(); ++i) {
    for (size_t j = 0; j < CHANNELS_PER_LINK; ++j) {
      mask_vec.push_back(0);
    }
    m_index[link_idx[i]] = channels;
    channels += CHANNELS_PER_LINK;
  }
}


std::unique_ptr<daqdataformats::TriggerRecord>
ChannelMask::run_wib2frame(std::unique_ptr<daqdataformats::TriggerRecord> record,
                             std::shared_ptr<ChannelMap>& map,
                             const std::string& kafka_address)
{
  return std::move(record);
  auto wibframes = decode<detdataformats::wib2::WIB2Frame>(*record);

  if (wibframes.size() == 0) {
    // throw issue
    TLOG() << "Found no frames";
    return std::move(record);
  }

  // Remove empty fragments
  for (auto& vec : wibframes)
    if (!vec.second.size())
      wibframes.erase(vec.first);

  // Get all the keys
  std::vector<int> keys;
  for (auto& [key, value] : wibframes) {
    keys.push_back(key);
  }

  std::vector<uint32_t> mask;
  for (auto& [key, frames] : wibframes) {
    mask.push_back(frames.front()->header.link_mask);
    TLOG() << key << " " << mask.back();
  }

  // Fill for every frame, outer loop so it is done frame by frame
  size_t ifr = 0;
  for (size_t ikey = 0; ikey < keys.size(); ++ikey) {
    auto fr = wibframes[keys[ikey]][ifr];

    auto mask = fr->header.link_mask;
    for (int imask = 0; imask < 8; ++imask) {
      auto masked = mask & (1 << imask);
      if (masked) {
        for (int ich = 0; ich < 32; ++ich) {
          mask_vec[get_local_index(ich + imask * 32, keys[ikey])] = 1;
        }
      }

    }
  }
  transmit(kafka_address, map, "testdunedqm",
           record->get_header_ref().get_run_number(),
           record->get_header_ref().get_trigger_timestamp());
  clean();
  return std::move(record);
}

std::unique_ptr<daqdataformats::TriggerRecord>
ChannelMask::run(std::unique_ptr<daqdataformats::TriggerRecord> record,
                   std::atomic<bool>&,
                   std::shared_ptr<ChannelMap>& map,
                   std::string& frontend_type,
                   const std::string& kafka_address)
{
  if (frontend_type == "wib2") {
    set_is_running(true);
    auto ret = run_wib2frame(std::move(record), map, kafka_address);
    set_is_running(false);
    return ret;
  }
  return record;
}

void
ChannelMask::transmit(const std::string& kafka_address,
                        std::shared_ptr<ChannelMap>& cmap,
                        const std::string& topicname,
                        int run_num,
                        time_t timestamp)
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

  for (auto& [plane, map] : channel_order) {
    std::stringstream output;
    output << datasource << ";" << dataname << ";" << run_num << ";" << subrun << ";" << event << ";" << timestamp
           << ";" << metadata << ";" << partition << ";" << app_name << ";" << 0 << ";" << plane << ";";
    for (auto& [offch, ch] : map) {
      output << offch << " ";
    }
    output << "\n";
    for (auto& [offch, pair] : map) {
      int link = pair.first;
      int ch = pair.second;
      output << mask_vec[get_local_index(ch, link)] << " ";
    }
    TLOG_DEBUG(5) << "Size of the message in bytes: " << output.str().size();
    KafkaExport(kafka_address, output.str(), topicname);
  }
}

void
ChannelMask::clean()
{
  for (size_t i = 0; i < mask_vec.size(); ++i) {
    mask_vec[i] = 0;
  }
}

int
ChannelMask::get_local_index(int ch, int link)
{
  return ch + m_index[link];
}

} // namespace dunedaq::dqm

#endif // DQM_SRC_CHANNELMASK_HPP_
