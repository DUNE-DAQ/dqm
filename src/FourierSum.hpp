/**
 * @file FourierSum.hpp Implementation of a summed Fourier transform
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
*/
#ifndef DQM_SRC_FOURIERSUM_HPP_
#define DQM_SRC_FOURIERSUM_HPP_

// DQM
#include "AnalysisModule.hpp"
#include "ChannelMap.hpp"
#include "Constants.hpp"
#include "Decoder.hpp"
#include "Exporter.hpp"
#include "dqm/Fourier.hpp"

#include "daqdataformats/TriggerRecord.hpp"

#include <fstream>
#include <ostream>
#include <string>
#include <vector>
#include <cstdlib>

namespace dunedaq::dqm {

class FourierSum : public AnalysisModule
{
  std::string m_name;
  std::vector<Fourier> fouriervec;
  std::vector<Fourier> fouriersum;
  size_t m_size;
  int m_npoints;
  std::map<int, int> m_index;

public:
  FourierSum(std::string name, int size, double inc, int npoints);
  FourierSum(std::string name, int size, std::vector<int>& link_idx, double inc, int npoints);

  void run(std::unique_ptr<daqdataformats::TriggerRecord> record, std::unique_ptr<ChannelMap> &map, std::string kafka_address="");
  void transmit(std::string &kafka_address, std::unique_ptr<ChannelMap> &cmap, const std::string& topicname, int run_num, time_t timestamp);
  void clean();
  void fill(int ch, double value);
  void fill(int ch, int link, double value);
  int get_local_index(int ch, int link);

};

FourierSum::FourierSum(std::string name, int size, double inc, int npoints)
  : m_name(name),
    m_size(size),
    m_npoints(npoints)
{
  for (size_t i = 0; i < m_size; ++i) {
    fouriervec.emplace_back(Fourier(inc, npoints));
  }
  for (size_t i = 0; i < 4; ++i) {
    fouriersum.emplace_back(Fourier(inc, npoints));
  }
}

FourierSum::FourierSum(std::string name, int size, std::vector<int>& link_idx, double inc, int npoints)
  : m_name(name),
    m_size(size),
    m_npoints(npoints)
{
  for (size_t i = 0; i < m_size; ++i) {
    fouriervec.emplace_back(Fourier(inc, npoints));
  }
  for (size_t i = 0; i < 4; ++i) {
    fouriersum.emplace_back(Fourier(inc, npoints));
  }
  int channels = 0;
  for (size_t i = 0; i < link_idx.size(); ++i) {
    m_index[link_idx[i]] = channels;
    channels += CHANNELS_PER_LINK;
  }
}
void
FourierSum::run(std::unique_ptr<daqdataformats::TriggerRecord> record, std::unique_ptr<ChannelMap> &map, std::string kafka_address)
{
  m_run_mark.store(true);
  dunedaq::dqm::Decoder dec;
  auto wibframes = dec.decode(*record);
  // std::uint64_t timestamp = 0; // NOLINT(build/unsigned)

  for (auto& [key, value] : wibframes) {
    for (auto& fr : value) {
      for (size_t ich = 0; ich < CHANNELS_PER_LINK; ++ich) {
        fill(ich, key, fr->get_channel(ich));
      }
    }
  }

  //------MODIFIED-------//
  auto channel_order = map->get_map();

  std::vector<std::vector<double>> plane_sums;

  for (auto& [plane, map] : channel_order)
  {
    std::vector<double> plane_sum(m_npoints, 0.);

    for (size_t i = 0; i < m_npoints; i++)
    {
      for (auto& [offch, pair]: map)
      {
        int link = pair.first;
        int ch   = pair.second;
        int local_index = get_local_index(ch, link);

        plane_sum[i] += fouriervec[local_index].m_data[i];
      }
    }
    plane_sums.push_back(plane_sum);
  }

  std::vector<double> allplane_sum(m_npoints, 0.);

  for (size_t i = 0; i < 3; i++)
  {
    for (size_t j = 0; j < m_npoints; j++)
    {
      fouriersum[i].fill(plane_sums[i][j]);
      allplane_sum[j] += plane_sums[i][j];
    }
  }

  for (size_t i = 0; i < m_npoints; i++)
  {
    fouriersum[3].fill(allplane_sum[i]);
  }


    /*
    for (auto& [offch, pair] : map)
    {
      int link = pair.first;
      int ch = pair.second;
      int local_index = get_local_index(ch, link);

      for (size_t i = 0; i < npoints; i++)
      {
        if (!vector_filled)
        {
          plane_sum.push_back(fouriervec[local_index].m_data[i]);
          if (i == npoints-1) vector_filled = true;
        }
        else
        {
          plane_sum[i] += fouriervec[local_index].m_data[i];
        }
      }
      

      for (size_t i = 0; i < m_size; i++)
      {
        fouriersum[plane].m_data[i] += fouriervec[local_index].m_data[i];
        fouriersum[3].m_data[i] += fouriervec[local_index].m_data[i];
      }
      TLOG() << "Time series length = " << fouriersum[3].m_data.size() << std::endl;
    }
    */
  
  for (size_t i = 0; i < 4; i++)
  {
    fouriersum[i].compute_fourier_normalized();
  }
  //-------------------//

  transmit(kafka_address, map, "testdunedqm", record->get_header_ref().get_run_number(), record->get_header_ref().get_trigger_timestamp());

  m_run_mark.store(false);
}

void
FourierSum::transmit(std::string &kafka_address, std::unique_ptr<ChannelMap> &cmap, const std::string& topicname, int run_num, time_t timestamp)
{
  // Placeholders
  std::string dataname = m_name;
  std::string metadata = "";
  int subrun = 0;
  int event = 0;
  std::string partition = getenv("DUNEDAQ_PARTITION");
  std::string app_name = getenv("DUNEDAQ_APPLICATION_NAME");
  //std::string datasource = partition + "_" + app_name;
  std::string datasource = "fftsum_test";


  auto freq = fouriervec[0].get_frequencies();
  // One message is sent for every plane
  for (int plane = 0; plane < 4; plane++)
  {
    std::stringstream output;
    output << datasource << ";" << dataname << ";" << run_num << ";" << subrun
           << ";" << event << ";" << timestamp << ";" << metadata << ";"
           << partition << ";" << app_name << ";" << 0 << ";" << plane << ";";
    //output << "\n";
    for (size_t i=0; i < freq.size(); ++i) 
    {
      int integer_freq = (int) freq[i];
      output << integer_freq << " ";
    }
    output << "\n";
    output << "Summed FFT\n";
    for (size_t i = 0; i < freq.size(); ++i)
    {
      output << fouriersum[plane].get_transform(i) << " ";
    }
    output << "\n";
    TLOG() << output.str() << std::endl;
    KafkaExport(kafka_address, output.str(), topicname);
  }

  clean();
}

void
FourierSum::clean()
{
  for (size_t ich = 0; ich < m_size; ++ich) {
    fouriervec[ich].clean();
  }
}

void
FourierSum::fill(int ch, double value)
{
  fouriervec[ch].fill(value);
}

void
FourierSum::fill(int ch, int link, double value)
{
  fouriervec[ch + m_index[link]].fill(value);
}

int
FourierSum::get_local_index(int ch, int link)
{
  return ch + m_index[link];
}


} // namespace dunedaq::dqm

#endif // DQM_SRC_FOURIERSUM_HPP_
