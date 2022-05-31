/**
 * @file DFModule.hpp Class for running algorithms on the TRs from DF
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_DFMODULE_HPP_
#define DQM_SRC_DFMODULE_HPP_

// DQM
#include "AnalysisModule.hpp"
#include "ChannelMap.hpp"
#include "Constants.hpp"
#include "dqm/DQMIssues.hpp"
#include "HistContainer.hpp"
#include "FourierContainer.hpp"

#include "daqdataformats/TriggerRecord.hpp"

#include <string>

namespace dunedaq::dqm {

class DFModule : public AnalysisModule
{

public:
  DFModule(bool enable_hist, bool enable_mean_rms, bool enable_fourier, bool enable_fourier_sum,
           int clock_frequency, std::vector<int>& ids, int num_frames);

  bool m_enable_hist, m_enable_mean_rms, m_enable_fourier, m_enable_fourier_sum;
  int m_clock_frequency;
  std::unique_ptr<daqdataformats::TriggerRecord>
  run(std::unique_ptr<daqdataformats::TriggerRecord> record,
      std::atomic<bool>& run_mark,
      std::shared_ptr<ChannelMap>& map,
      std::string& frontend_type,
      const std::string& kafka_address);

private:

  std::shared_ptr<HistContainer> m_hist, m_mean_rms;
  std::shared_ptr<FourierContainer> m_fourier, m_fourier_sum;

  std::vector<int> m_ids;

  int m_num_frames;

};

DFModule::DFModule(bool enable_hist, bool enable_mean_rms, bool enable_fourier, bool enable_fourier_sum,
                   int clock_frequency, std::vector<int>& ids, int num_frames) :
    m_enable_hist(enable_hist),
    m_enable_mean_rms(enable_mean_rms),
    m_enable_fourier(enable_fourier),
    m_enable_fourier_sum(enable_fourier_sum),
    m_clock_frequency(clock_frequency),
    m_ids(ids),
    m_num_frames(num_frames)

{
  if (m_enable_hist) {
    m_hist = std::make_shared<HistContainer>(
                                                 "raw_display", CHANNELS_PER_LINK * m_ids.size(), m_ids, 100, 0, 17000, false);
  }
  if (m_enable_mean_rms) {
    m_mean_rms = std::make_shared<HistContainer>(
                                                     "rmsm_display", CHANNELS_PER_LINK * m_ids.size(), m_ids, 100, 0, 17000, true);
  }
  if (m_enable_fourier) {
    m_fourier = std::make_shared<FourierContainer>("fft_display",
                                                    CHANNELS_PER_LINK * m_ids.size(),
                                                    m_ids,
                                                    1. / m_clock_frequency * TICKS_BETWEEN_TIMESTAMP,
                                                    m_num_frames);
  }
  if (m_enable_fourier_sum) {
    m_fourier_sum = std::make_shared<FourierContainer>("fft_sums_display",
                                                       4,
                                                       m_ids,
                                                       1. / m_clock_frequency * TICKS_BETWEEN_TIMESTAMP,
                                                       m_num_frames,
                                                       true);
  }
}

std::unique_ptr<daqdataformats::TriggerRecord>
DFModule::run(std::unique_ptr<daqdataformats::TriggerRecord> record,
              std::atomic<bool>& run_mark,
              std::shared_ptr<ChannelMap>& map,
              std::string& frontend_type,
              const std::string& kafka_address)
{
  set_is_running(true);

  std::vector<std::shared_ptr<AnalysisModule>> list {m_hist, m_mean_rms, m_fourier, m_fourier_sum};
  std::vector<bool> will_run {m_enable_hist, m_enable_mean_rms, m_enable_fourier, m_enable_fourier_sum};

  for(size_t i=0; i < list.size(); ++i) {
    if (!will_run[i]) continue;
    if (!run_mark) {
      set_is_running(false);
      return std::move(record);
    }
    record = list[i]->run(std::move(record), run_mark, map, frontend_type, kafka_address);
  }

  set_is_running(false);
  return std::move(record);
}


} // namespace dunedaq::dqm

#endif // DQM_SRC_DFMODULE_HPP_
