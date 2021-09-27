/**
 * @file Exporter.hpp Exporter...
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_CHANNELMAPPER_HPP_
#define DQM_SRC_CHANNELMAPPER_HPP_

// DQM
#include "dqm/Types.hpp"

#include "dataformats/TriggerRecord.hpp"

#include "chmap/PdspChannelMapService.hpp"

//Global channel map object to avoid repeated creation and deletion
std::unique_ptr<swtpg::PdspChannelMapService> channelMap;
const char* readout_share_cstr = getenv("READOUT_SHARE");
std::string readout_share(readout_share_cstr);
std::string channel_map_rce = readout_share +  "/config/protoDUNETPCChannelMap_RCE_v4.txt";
std::string channel_map_felix = readout_share + "/config/protoDUNETPCChannelMap_FELIX_v4.txt";

namespace dunedaq::dqm
{

unsigned int getOfflineChannel(const dunedaq::dataformats::WIBFrame* frame,
                               unsigned int ch) // NOLINT(build/unsigned)
{
  //Acquire channel map (N.B. - this would be better optimised if done once in the algorithm that runs this function, but for the sake of keeping the channel mapping code in one place it's currently here)
  channelMap.reset(new swtpg::PdspChannelMapService(channel_map_rce, channel_map_felix));

  // handle 256 channels on two fibers -- use the channel
  // map that assumes 128 chans per fiber (=FEMB) (Copied
  // from PDSPTPCRawDecoder_module.cc)
  int crate = frame->get_wib_header()->crate_no;
  int slot = frame->get_wib_header()->slot_no;
  int fiber = frame->get_wib_header()->fiber_no;

  unsigned int fiberloc = 0; // NOLINT(build/unsigned)
  if (fiber == 1) {
    fiberloc = 1;
  } else if (fiber == 2) {
    fiberloc = 3;
  } else {
    fiberloc = 1;
  }

  unsigned int chloc = ch; // NOLINT(build/unsigned)
  if (chloc > 127) {
    chloc -= 128;
    fiberloc++;
  }

  unsigned int crateloc = crate; // NOLINT(build/unsigned)
  unsigned int offline =         // NOLINT(build/unsigned)
  channelMap->GetOfflineNumberFromDetectorElements(crateloc, slot, fiberloc, chloc, swtpg::PdspChannelMapService::kFELIX);
  
return offline;
}

unsigned int LocalWireNumber(unsigned int offline)
{
  //Acquire channel map (N.B. - this would be better optimised if done once in the algorithm that runs this function, but for the sake of keeping the channel mapping code in one place it's currently here)
  channelMap.reset(new swtpg::PdspChannelMapService(channel_map_rce, channel_map_felix));

  unsigned int apa =  channelMap->APAFromOfflineChannel(offline);
  unsigned int plane = channelMap->PlaneFromOfflineChannel(offline);

  int apa_channel = offline - apa*2560;
  unsigned int local_channel = apa_channel - plane*800;

  return local_channel;
}

} //namespace dqm

#endif // DQM_SRC_CHANNELMAPPER_HPP_
