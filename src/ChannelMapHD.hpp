/**
 * @file ChannelMapHD.hpp Implementation of the channel map for the horizontal drift
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_CHANNELMAPHD_HPP_
#define DQM_SRC_CHANNELMAPHD_HPP_

// DQM
#include "ChannelMap.hpp"
#include "Decoder.hpp"
#include "Constants.hpp"
#include "dqm/DQMIssues.hpp"

#include "daqdataformats/TriggerRecord.hpp"
#include "detchannelmaps/TPCChannelMap.hpp"

#include <stdlib.h>
#include <set>

namespace dunedaq::dqm {

class ChannelMapHD : public ChannelMap {
  std::map<int, std::map<int, std::pair<int, int>>> m_map;

public:
  ChannelMapHD();
  int get_channel(int channel);
  int get_plane(int channel);
  bool is_filled();
  void fill(daqdataformats::TriggerRecord &tr);
  std::map<int, std::map<int, std::pair<int, int>>> get_map();
};

ChannelMapHD::ChannelMapHD()
{
  m_chmap_service = dunedaq::detchannelmaps::make_map("ProtoDUNESP1ChannelMap");
}

std::map<int, std::map<int, std::pair<int, int>>>
ChannelMapHD::get_map()
{
  return m_map;
}

void
ChannelMapHD::fill(daqdataformats::TriggerRecord &tr)
{
  if (is_filled()) {
    TLOG_DEBUG(5) << "ChannelMapHD already filled";
    return;
  }

  dunedaq::dqm::Decoder dec;
  auto wibframes = dec.decode(tr);

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
      auto tmp = std::make_tuple<int, int, int>((int)crate, (int)slot, (int)fiber);
      if (frame_numbers.find(tmp) == frame_numbers.end()) {
        frame_numbers.insert(tmp);
      }
      else {
        continue;
      }
      for (int ich=0; ich < CHANNELS_PER_LINK; ++ich) {
        auto channel = m_chmap_service->get_offline_channel_from_crate_slot_fiber_chan(crate, slot, fiber, ich);
        auto plane = m_chmap_service->get_plane_from_offline_channel(channel);
        m_map[plane][channel] = {key, ich};
      }
    }
  }
  TLOG_DEBUG(10) << "Channel mapping done, size of the map is " << m_map[0].size() << " " << m_map[1].size() << " " << m_map[2].size();

  TLOG_DEBUG(5) << "Channel Map for the HD created";
  m_is_filled = true;

}

bool
ChannelMapHD::is_filled()
{
  return m_is_filled;
}

} // namespace dunedaq::dqm

#endif // DQM_SRC_CHANNELMAPHD_HPP_
