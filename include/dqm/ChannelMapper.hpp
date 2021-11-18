/**
 * @file ChannelMapper.hpp ChannelMapper...
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_CHANNELMAPPER_HPP_
#define DQM_SRC_CHANNELMAPPER_HPP_

// DQM
#include "daqdataformats/TriggerRecord.hpp"

#include "readout/chmap/PdspChannelMapService.hpp"

//Global channel map object to avoid repeated creation and deletion

struct ChannelInfo
{
  int APA = 0;
  int Plane = 0;
  int Wire = 0;
  std::string GeoID = "NULL";
  std::string Application = "NULL";
  std::string Partition = "NULL";
};

namespace dunedaq::dqm
{

unsigned int getOfflineChannel(swtpg::PdspChannelMapService& channelMap, 
                               const dunedaq::detdataformats::wib::WIBFrame* frame,
                               unsigned int ch) // NOLINT(build/unsigned)
{
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
  channelMap.GetOfflineNumberFromDetectorElements(crateloc, slot, fiberloc, chloc, swtpg::PdspChannelMapService::kFELIX);
  
return offline;
}

unsigned int LocalWireNumber(swtpg::PdspChannelMapService& channelMap, 
                             unsigned int offline)
{
  unsigned int apa =  channelMap.APAFromOfflineChannel(offline);
  unsigned int plane = channelMap.PlaneFromOfflineChannel(offline);

  int apa_channel = offline - apa*2560;
  unsigned int local_channel = apa_channel - plane*800;

  return local_channel;
}

unsigned int GetPlane(swtpg::PdspChannelMapService& channelMap, 
                             unsigned int offline)
{
  unsigned int plane = channelMap.PlaneFromOfflineChannel(offline);
  return plane;
}

} //namespace dqm

#endif // DQM_SRC_CHANNELMAPPER_HPP_
