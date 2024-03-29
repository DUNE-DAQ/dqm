/**
 * @file FourierContainer.hpp Implementation of a container of Fourier objects
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_FOURIERCONTAINER_HPP_
#define DQM_SRC_FOURIERCONTAINER_HPP_

// DQM
#include "dqm/AnalysisModule.hpp"
#include "dqm/ChannelMap.hpp"
#include "dqm/Constants.hpp"
#include "dqm/Decoder.hpp"
#include "dqm/Exporter.hpp"
#include "dqm/algs/Fourier.hpp"
#include "dqm/Issues.hpp"
#include "dqm/DQMFormats.hpp"
#include "dqm/DQMLogging.hpp"

#include "daqdataformats/TriggerRecord.hpp"

#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dunedaq::dqm {

using logging::TLVL_WORK_STEPS;

class FourierContainer : public AnalysisModule
{
  std::string m_name;
  std::vector<Fourier> fouriervec;
  size_t m_size;
  int m_npoints;
  std::map<int, int> m_index;
  bool m_global_mode;

public:
  FourierContainer(std::string name, int size, double inc, int npoints);
  FourierContainer(std::string name, int size, std::vector<int>& link_idx, double inc, int npoints, bool global_mode=false);

  void run(std::shared_ptr<daqdataformats::TriggerRecord> record,
      DQMArgs& args, DQMInfo& info) override;

  template <class T>
  void run_(std::shared_ptr<daqdataformats::TriggerRecord> record,
       DQMArgs& args, DQMInfo& info);


  // void transmit(const std::string& kafka_address,
  //               std::shared_ptr<ChannelMap> cmap,
  //               const std::string& topicname,
  //               int run_num,
  //               time_t timestamp);
  void transmit_global(const std::string &kafka_address,
                       std::shared_ptr<ChannelMap> cmap,
                       const std::string& topicname,
                       int run_num);
  void clean();
  void fill(int ch, double value);
  void fill(int ch, int link, double value);
  int get_local_index(int ch, int link);
};

FourierContainer::FourierContainer(std::string name, int size, double inc, int npoints)
  : m_name(name)
  , m_size(size)
  , m_npoints(npoints)
{
  for (size_t i = 0; i < m_size; ++i) {
    fouriervec.emplace_back(Fourier(inc, npoints));
  }

}

FourierContainer::FourierContainer(std::string name, int size, std::vector<int>& link_idx, double inc, int npoints, bool global_mode)
  : m_name(name)
  , m_size(size)
  , m_npoints(npoints)
  , m_global_mode(global_mode)
{
  for (size_t i = 0; i < m_size; ++i) {
    fouriervec.emplace_back(Fourier(inc, npoints));
  }
  int channels = 0;
  for (size_t i = 0; i < link_idx.size(); ++i) {
    m_index[link_idx[i]] = channels;
    channels += CHANNELS_PER_LINK;
  }
}

template <class T>
void
FourierContainer::run_(std::shared_ptr<daqdataformats::TriggerRecord> record,
                       DQMArgs& args, DQMInfo& info)
{
  auto start = std::chrono::steady_clock::now();
  auto map = args.map;
  auto frames = decode<T>(record, args.max_frames);
  auto pipe = Pipeline<T>({"remove_empty", "check_empty", "make_same_size", "check_timestamps_aligned"});
  bool valid_data = pipe(frames);
  if (!valid_data) {
    return;
  }

  //auto size = frames.begin()->second.size();

  // Normal mode, fourier transform for every channel
  if (!m_global_mode) {
    for (auto& [key, value] : frames) {
      for (auto& fr : value) {
        for (size_t ich = 0; ich < CHANNELS_PER_LINK; ++ich) {
          fill(ich, key, get_adc<T>(fr, ich));
        }
      }
    }
    for (size_t ich = 0; ich < m_size; ++ich) {
      fouriervec[ich].compute_fourier_transform();
    }
    // transmit(args.kafka_address,
    //          map,
    //          args.kafka_topic,
    //          record->get_header_ref().get_run_number(),
    //          record->get_header_ref().get_trigger_timestamp());
    auto stop = std::chrono::steady_clock::now();
    info.fourier_channel_time_taken.store(std::chrono::duration_cast<std::chrono::milliseconds>(stop-start).count());
    info.fourier_channel_times_run++;
  }

  // Global mode means adding everything in planes and then all together
  else {
    // Initialize the vectors with zeroes, the last one can be done by summing
    // the resulting transform
    for (size_t i = 0; i < m_size - 1; ++i) {
      fouriervec[i].m_data = std::vector<double> ((*frames.begin()).second.size(), 0);
    }

    auto channel_order = map->get_map();
    for (const auto& [plane, map] : channel_order) {
      if (plane > 3 ) {
        ers::error(InvalidInput(ERS_HERE, "Plane " + std::to_string(plane) + " is not a valid plane"));
        continue;
      }
      for (const auto& [offch, pair] : map) {
        int link = pair.first;
        int ch = pair.second;
        if (frames.find(link) == frames.end()) {
          ers::error(InvalidInput(ERS_HERE, "Link " + std::to_string(link) + " was not present in data"));
          continue;
        }
        for (size_t iframe = 0; iframe < frames[link].size(); ++iframe) {
          fouriervec[plane].m_data[iframe] += get_adc<T>(frames[link][iframe], ch);
        }
      }
    }

    for (size_t ich = 0; ich < m_size - 1; ++ich) {
      if (!args.run_mark.get()) {
        return;
      }
      fouriervec[ich].compute_fourier_transform();
    }
    // The last one corresponds can be obtained as the sum of the ones for the planes
    // since the fourier transform is linear
    std::vector<std::complex<double>> transform(fouriervec[0].m_transform);
    fouriervec[m_size-1].m_npoints = fouriervec[0].m_npoints;
    for (size_t i = 0; i < fouriervec[0].m_transform.size(); ++i) {
      transform[i] += fouriervec[1].m_transform[i] + fouriervec[2].m_transform[i];
    }
    fouriervec[m_size-1].m_transform = transform;
    transmit_global(args.kafka_address,
                    map,
                    args.kafka_topic,
                    record->get_header_ref().get_run_number());
    auto stop = std::chrono::steady_clock::now();
    info.fourier_plane_time_taken.store(std::chrono::duration_cast<std::chrono::milliseconds>(stop-start).count());
    info.fourier_plane_times_run++;
  }

}


void
FourierContainer::run(std::shared_ptr<daqdataformats::TriggerRecord> record,
                      DQMArgs& args, DQMInfo& info)
{
  TLOG(TLVL_WORK_STEPS) << "Running Fourier Transform with frontend_type = " << args.frontend_type;
  auto frontend_type = args.frontend_type;
  auto run_mark = args.run_mark;
  auto map = args.map;
  auto kafka_address = args.kafka_address;
  if (frontend_type == "wib") {
    set_is_running(true);
    run_<fddetdataformats::WIBFrame>(std::move(record), args, info);
    set_is_running(false);
  }
  else if (frontend_type == "wib2") {
    set_is_running(true);
    run_<fddetdataformats::WIB2Frame>(std::move(record), args, info);
    set_is_running(false);
  }
}

// void
// FourierContainer::transmit(const std::string& kafka_address,
//                            std::shared_ptr<ChannelMap> cmap,
//                            const std::string& topicname,
//                            int run_num,
//                            time_t timestamp)
// {

  // // Placeholders
  // std::string dataname = m_name;
  // std::string partition = getenv("DUNEDAQ_PARTITION");
  // std::string app_name = getenv("DUNEDAQ_APPLICATION_NAME");
  // std::string datasource = partition + "_" + app_name;

  // // One message is sent for every plane
  // auto channel_order = cmap->get_map();
  // for (auto& [plane, map] : channel_order) {
  //   std::stringstream output;
  //   output << "{";
  //   output << "\"source\": \"" << datasource << "\",";
  //   output << "\"run_number\": \"" << run_num << "\",";
  //   output << "\"partition\": \"" << partition << "\",";
  //   output << "\"app_name\": \"" << app_name << "\",";
  //   output << "\"plane\": \"" << plane << "\",";
  //   output << "\"algorithm\": \"" << "std" << "\"";
  //   output << "}\n\n\n";
  //   std::vector<float> freqs = fouriervec[0].get_frequencies();
  //   auto bytes = serialization::serialize(freqs, serialization::kMsgPack);
  //   for (auto& b : bytes) {
  //     output << b;
  //   }
  //   output << "\n\n\n";
  //   std::vector<float> values;
  //   for (auto& [offch, pair] : map) {
  //     int link = pair.first;
  //     int ch = pair.second;
  //     values.push_back(histvec[get_local_index(ch, link)].std());
  //   }
  //   bytes = serialization::serialize(values, serialization::kMsgPack);
  //   for (auto& b : bytes) {
  //     output << b;
  //   }
  //   TLOG_DEBUG(5) << "Size of the message in bytes: " << output.str().size();
  //   KafkaExport(kafka_address, output.str(), topicname);
  // }

  // auto freq = fouriervec[0].get_frequencies();
  // // One message is sent for every plane
  // auto channel_order = cmap->get_map();
  // for (auto& [plane, map] : channel_order) {
  //   std::stringstream output;
  //   output << datasource << ";" << dataname << ";" << run_num << ";" << subrun << ";" << event << ";" << timestamp
  //          << ";" << metadata << ";" << partition << ";" << app_name << ";" << 0 << ";" << plane << ";";
  //   for (auto& [offch, pair] : map) {
  //     output << offch << " ";
  //   }
  //   output << "\n";
  //   for (size_t i = 0; i < freq.size(); ++i) {
  //     output << freq[i] << "\n";
  //     for (auto& [offch, pair] : map) {
  //       int link = pair.first;
  //       int ch = pair.second;
  //       output << fouriervec[get_local_index(ch, link)].get_transform_at(i) << " ";
  //     }
  //     output << "\n";
  //   }
  //   TLOG_DEBUG(5) << "Size of the message in bytes: " << output.str().size();
  //   KafkaExport(kafka_address, output.str(), topicname);
  // }

  // clean();
// }

void
FourierContainer::transmit_global(const std::string& kafka_address,
                                  std::shared_ptr<ChannelMap>,
                                  const std::string& topicname,
                                  int run_num)
{
  // Placeholders
  std::string dataname = m_name;
  std::string partition = getenv("DUNEDAQ_PARTITION");
  std::string app_name = getenv("DUNEDAQ_APPLICATION_NAME");
  std::string datasource = partition + "_" + app_name;

  // One message is sent for every plane
  for (int plane = 0; plane < 4; plane++) {
    std::stringstream output;
    output << "{";
    output << "\"source\": \"" << datasource << "\",";
    output << "\"run_number\": \"" << run_num << "\",";
    output << "\"partition\": \"" << partition << "\",";
    output << "\"app_name\": \"" << app_name << "\",";
    output << "\"plane\": \"" << plane << "\",";
    output << "\"algorithm\": \"" << "fourier_plane" << "\"";
    output << "}\n\n\n";
    std::vector<double> freqs = fouriervec[plane].get_frequencies();
    auto bytes = serialization::serialize(freqs, serialization::kMsgPack);
    for (auto& b : bytes) {
      output << b;
    }
    output << "\n\n\n";
    auto tmp = fouriervec[plane].get_transform();
    std::vector<double> values;
    values.reserve(tmp.size());
    for (const auto& v : tmp) {
      values.push_back(abs(v));
    }
    bytes = serialization::serialize(values, serialization::kMsgPack);
    for (auto& b : bytes) {
      output << b;
    }
    TLOG_DEBUG(5) << "Size of the message in bytes: " << output.str().size();
    KafkaExport(kafka_address, output.str(), topicname);
  }

  clean();
}

void
FourierContainer::clean()
{
  for (size_t ich = 0; ich < m_size; ++ich) {
    fouriervec[ich].clean();
  }
}

void
FourierContainer::fill(int ch, double value)
{
  fouriervec[ch].fill(value);
}

void
FourierContainer::fill(int ch, int link, double value)
{
  fouriervec[ch + m_index[link]].fill(value);
}

int
FourierContainer::get_local_index(int ch, int link)
{
  return ch + m_index[link];
}

} // namespace dunedaq::dqm

#endif // DQM_SRC_FOURIERCONTAINER_HPP_
