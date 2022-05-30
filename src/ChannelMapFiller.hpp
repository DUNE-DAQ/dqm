/**
 * @file ChannelMapFiller.hpp Implementation of an algorithm to fill the channel map
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_CHANNELMAPFILLER_HPP_
#define DQM_SRC_CHANNELMAPFILLER_HPP_

// DQM
#include "AnalysisModule.hpp"
// #include "ChannelMap.hpp"
#include "ChannelMap.hpp"
#include "ChannelMapHD.hpp"
#include "ChannelMapPD2HD.hpp"
#include "ChannelMapVD.hpp"

#include "daqdataformats/TriggerRecord.hpp"

#include <memory>
#include <string>

namespace dunedaq::dqm {

class ChannelMapFiller : public AnalysisModule
{
  std::string m_name;
  std::string m_cmap_name;

public:
  ChannelMapFiller(std::string name, std::string cmap_name);

  std::unique_ptr<daqdataformats::TriggerRecord>
  run(std::unique_ptr<daqdataformats::TriggerRecord> record,
      std::atomic<bool>& run_mark,
      std::shared_ptr<ChannelMap>& map,
      std::string& frontend_type,
      const std::string& kafka_address);
};

std::unique_ptr<daqdataformats::TriggerRecord>
ChannelMapFiller::run(std::unique_ptr<daqdataformats::TriggerRecord> record,
                      std::atomic<bool>&,
                      std::shared_ptr<ChannelMap>& map,
                      std::string&,
                      const std::string&)
{
  set_is_running(true);

  // Prevent running multiple times
  if (map->is_filled()) {
    return std::move(record);
  }

  if (m_cmap_name == "HD") {
    map.reset(new ChannelMapHD);
  }
  else if (m_cmap_name == "VD") {
    map.reset(new ChannelMapVD);
  }
  else if (m_cmap_name == "PD2HD") {
    map.reset(new ChannelMapPD2HD);
  }

  map->fill(*record);
  set_is_running(false);
  return std::move(record);
}

ChannelMapFiller::ChannelMapFiller(std::string name, std::string cmap_name)
  : m_name(name)
{
  if (cmap_name != "HD" && cmap_name != "VD" && cmap_name != "PD2HD") {
    TLOG() << "Wrong channel map name";
  } else {
    m_cmap_name = cmap_name;
  }
}

} // namespace dunedaq::dqm

#endif // DQM_SRC_CHANNELMAPFILLER_HPP_
