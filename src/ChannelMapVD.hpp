/**
 * @file ChannelMapVD.hpp Implementation of the channel map for the vertical drift
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_CHANNELMAPVD_HPP_
#define DQM_SRC_CHANNELMAPVD_HPP_

// DQM
#include "Constants.hpp"
#include "Decoder.hpp"
#include "dqm/DQMIssues.hpp"

#include "daqdataformats/TriggerRecord.hpp"
#include "detchannelmaps/TPCChannelMap.hpp"

#include <cstdlib>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace dunedaq::dqm {

typedef std::vector<int> vi;
typedef std::vector<vi> vvi;
typedef std::vector<vvi> vvvi;

class ChannelMapVD : public ChannelMap
{

  std::map<int, std::map<int, std::pair<int, int>>> m_map;

  // Three dimensional array of 4x2x256 (4 WIBS x 2 LINKS x 256 channels)
  // holding the offline channel corresponding to each combination of slot,
  // fiber and frame channel
  vvvi channelvec;
  vvvi planevec;

public:
  ChannelMapVD();
  void fill(daqdataformats::TriggerRecord& tr);
  std::map<int, std::map<int, std::pair<int, int>>> get_map();
};

ChannelMapVD::ChannelMapVD()
{
  channelvec = vvvi(4, vvi(2, vi(256, -1)));
  planevec = vvvi(4, vvi(2, vi(256, -1)));
  m_chmap_service = dunedaq::detchannelmaps::make_map("VDColdboxChannelMap");
}

std::map<int, std::map<int, std::pair<int, int>>>
ChannelMapVD::get_map()
{
  return m_map;
}

void
ChannelMapVD::fill(daqdataformats::TriggerRecord& tr)
{

  if (is_filled()) {
    TLOG_DEBUG(5) << "ChannelMapVD already filled";
    return;
  }

  Decoder dec;
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
      auto tmp = std::make_tuple(crate, slot, fiber);
      if (frame_numbers.find(tmp) == frame_numbers.end()) {
        frame_numbers.insert(tmp);
      } else {
        continue;
      }
      for (int ich = 0; ich < CHANNELS_PER_LINK; ++ich) {
        auto channel = m_chmap_service->get_offline_channel_from_crate_slot_fiber_chan(crate, slot, fiber, ich);
        auto plane = m_chmap_service->get_plane_from_offline_channel(channel);

        m_map[plane][channel] = { key, ich };
      }
    }
  }

  // Delete the default value to make sure it's not sent
  if (m_map.find(-1) != m_map.end()) {
    m_map.erase(-1);
  }
  // Add this entry because it's the value of the plane for the disconnected
  // channels
  if (m_map.find(9999) != m_map.end()) {
    m_map.erase(9999);
  }

  TLOG_DEBUG(5) << "Channel Map for the VD created";
  m_is_filled = true;
}

} // namespace dunedaq::dqm

#endif // DQM_SRC_CHANNELMAPVD_HPP_
