/**
 * @file Decoder.hpp Implementation of decoders used in DQM analysis modules
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_DECODER_HPP_
#define DQM_SRC_DECODER_HPP_

#include "daqdataformats/Fragment.hpp"
#include "daqdataformats/TriggerRecord.hpp"
#include "detdataformats/wib/WIBFrame.hpp"

#include <climits>
#include <memory>
#include <vector>

namespace dunedaq {
namespace dqm {

class Decoder
{
public:
  /**
   * @brief Decode a Trigger Record
   * @param record The Trigger Record to be decoded
   * @return A map whose keys are the GeoID indexes and the values are
   *         vectors of pointers to the wibframes
   */
  std::map<int, std::vector<detdataformats::wib::WIBFrame*>> decode(dunedaq::daqdataformats::TriggerRecord& record);
};

std::map<int, std::vector<detdataformats::wib::WIBFrame*>>
decodewib(daqdataformats::TriggerRecord& record)
{
  std::vector<std::unique_ptr<daqdataformats::Fragment>>& fragments = record.get_fragments_ref();

  std::map<int, std::vector<detdataformats::wib::WIBFrame*>> wibframes;

  for (auto& fragment : fragments) {
    auto id = fragment->get_element_id();
    auto element_id = id.element_id;
    int num_chunks = (fragment->get_size() - sizeof(daqdataformats::FragmentHeader)) / sizeof(detdataformats::wib::WIBFrame);
    std::vector<detdataformats::wib::WIBFrame*> tmp;
    for (int i = 0; i < num_chunks; ++i) {
      detdataformats::wib::WIBFrame* frame =
        reinterpret_cast<detdataformats::wib::WIBFrame*>(static_cast<char*>(fragment->get_data()) + (i * 464)); // NOLINT
      tmp.push_back(frame);
    }
    wibframes[element_id] = tmp;
  }

  return wibframes;
}

std::map<int, std::vector<detdataformats::wib::WIBFrame*>>
Decoder::decode(daqdataformats::TriggerRecord& record)
{
  return decodewib(record);
}

} // namespace dqm
} // namespace dunedaq

#endif // DQM_SRC_DECODER_HPP_
