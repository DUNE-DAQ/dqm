/**
 * @file ChannelMapPD2HD.hpp Implementation of the channel map for the horizontal drift
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_CHANNELMAPPD2HD_HPP_
#define DQM_SRC_CHANNELMAPPD2HD_HPP_

// DQM
#include "ChannelMap.hpp"
#include "Constants.hpp"
#include "Decoder.hpp"
#include "dqm/DQMIssues.hpp"

#include "daqdataformats/TriggerRecord.hpp"
#include "detchannelmaps/TPCChannelMap.hpp"
#include "logging/Logging.hpp"

#include <cstdlib>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace dunedaq::dqm {

class ChannelMapPD2HD : public ChannelMap
{
  std::map<int, std::map<int, std::pair<int, int>>> m_map;

public:
  ChannelMapPD2HD();
  int get_channel(int channel);
  int get_plane(int channel);
  bool is_filled();
  void fill(daqdataformats::TriggerRecord& record);
  std::map<int, std::map<int, std::pair<int, int>>> get_map();
};

ChannelMapPD2HD::ChannelMapPD2HD()
{
  m_chmap_service = dunedaq::detchannelmaps::make_map("PD2HDChannelMap");
}

std::map<int, std::map<int, std::pair<int, int>>>
ChannelMapPD2HD::get_map()
{
  return m_map;
}

void
ChannelMapPD2HD::fill(daqdataformats::TriggerRecord& record)
{
  if (is_filled()) {
    TLOG_DEBUG(5) << "ChannelMapPD2HD already filled";
    return;
  }

  auto wibframes = decode<detdataformats::wib::WIBFrame>(record);

  // If we get no frames then return and since
  // the map is not filled it will run again soon
  if (wibframes.size() == 0)
    return;

  std::set<std::tuple<int, int, int>> frame_numbers;
  for (auto& [key, value] : wibframes) {
    // This is one link so we push back one element to m_map
    for (auto& fr : value) {
      int crate = fr->get_wib_header()->crate_no;
      int slot = fr->get_wib_header()->slot_no;
      int fiber = fr->get_wib_header()->fiber_no;
      auto tmp = std::make_tuple(crate, slot, fiber);
      if (frame_numbers.find(tmp) == frame_numbers.end()) {
        frame_numbers.insert(tmp);
      } else {
        continue;
      }
      for (int ich = 0; ich < CHANNELS_PER_LINK; ++ich) {
        auto channel = m_chmap_service->get_offline_channel_from_crate_slot_fiber_chan(crate, slot, fiber, ich);
        auto plane = m_chmap_service->get_plane_from_offline_channel(channel);
        TLOG() << plane << " " << channel << " " << key << " " << ich;
        m_map[plane][channel] = { key, ich };
      }
    }
  }
  TLOG_DEBUG(10) << "Channel mapping done, size of the map is " << m_map[0].size() << " " << m_map[1].size() << " "
                 << m_map[2].size();

  TLOG_DEBUG(5) << "Channel Map for the HD created";
  m_is_filled = true;
}

bool
ChannelMapPD2HD::is_filled()
{
  return m_is_filled;
}

} // namespace dunedaq::dqm

#endif // DQM_SRC_CHANNELMAPPD2HD_HPP_
