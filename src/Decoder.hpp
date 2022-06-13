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
#include "detdataformats/wib2/WIB2Frame.hpp"

#include "ers/Issue.hpp"
#include "dqm/DQMIssues.hpp"

#include <climits>
#include <map>
#include <memory>
#include <vector>

namespace dunedaq {
namespace dqm {

template<class T>
std::map<int, std::vector<T*>>
decode_frame(daqdataformats::TriggerRecord& record)
{
  std::vector<std::unique_ptr<daqdataformats::Fragment>>& fragments = record.get_fragments_ref();

  std::map<int, std::vector<T*>> frames;

  for (auto& fragment : fragments) {
    if (fragment->get_fragment_type() != daqdataformats::FragmentType::kTPC) {
      continue;
    }
    auto id = fragment->get_element_id();
    auto element_id = id.element_id;
    int num_chunks =
      (fragment->get_size() - sizeof(daqdataformats::FragmentHeader)) / sizeof(T);
    std::vector<T*> tmp;
    for (int i = 0; i < num_chunks; ++i) {
      T* frame = reinterpret_cast<T*>( // NOLINT
      static_cast<char*>(fragment->get_data()) + (i * sizeof(T)));
      tmp.push_back(frame);
    }
    frames[element_id] = tmp;
  }

  return frames;
}

template<class T>
std::map<int, std::vector<T*>>
decode(dunedaq::daqdataformats::TriggerRecord& record) {
}

template <>
std::map<int, std::vector<detdataformats::wib::WIBFrame*>>
decode(dunedaq::daqdataformats::TriggerRecord& record) {
  return decode_frame<detdataformats::wib::WIBFrame>(record);
}

template <>
std::map<int, std::vector<detdataformats::wib2::WIB2Frame*>>
decode(dunedaq::daqdataformats::TriggerRecord& record) {
  return decode_frame<detdataformats::wib2::WIB2Frame>(record);
}


} // namespace dqm
} // namespace dunedaq

#endif // DQM_SRC_DECODER_HPP_
