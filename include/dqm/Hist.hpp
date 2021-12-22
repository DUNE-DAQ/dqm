/**
 * @file Hist.hpp Simple 1D histogram implementation
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_INCLUDE_DQM_HIST_HPP_
#define DQM_INCLUDE_DQM_HIST_HPP_

// DQM
#include "AnalysisModule.hpp"
#include "Decoder.hpp"
#include "Exporter.hpp"

#include "daqdataformats/TriggerRecord.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

/**
 * Basic 1D histogram that counts entries
 * It only supports uniform binning
 * Overflow and underflow is not supported yet
 */
namespace dunedaq::dqm {

class Hist
{

  int find_bin(double x) const;

public:
  double m_low, m_high, m_step_size;
  int m_nentries = 0;
  double m_sum = 0, m_sum_sq = 0, m_mean, m_std;
  bool m_mean_set = false, m_std_set = false;

  int m_steps;
  std::vector<int> m_entries;

  /**
   * @brief Hist constructor
   * @param steps Number of uniform bins
   * @param low Lower limit of the histogram
   * @param high Upper limit of the histogram
   */
  Hist(int steps, double low, double high);

  /**
   * @brief Add an entry to the histogram
   * @param x The number that is being added
   */
  int fill(double x);

  /**
   * @brief Save to a text file (for debugging purposes)
   * @param filename Name of the file where the histogram will be saved
   */
  void save(const std::string& filename) const;

  /**
   * @brief Save to a text file (for debugging purposes)
   * @param filehandle Handle of the file where the histogram will be saved
   */
  void save(std::ofstream& filehandle) const;

  bool is_running();
  void clean();

  double mean();
  double std();
};

Hist::Hist(int steps, double low, double high)
  : m_low(low)
  , m_high(high)
  , m_steps(steps)
{
  m_entries = std::vector<int>(steps, 0);
  m_step_size = (high - low) / steps;
}

int
Hist::find_bin(double x) const
{
  return (x - m_low) / m_step_size;
}

int
Hist::fill(double x)
{
  int bin = find_bin(x);
  // Underflow, do nothing
  if (bin < 0) {
    return -1;
  }

  // Overflow, do nothing
  if (bin >= m_steps) {
    return -1;
  }

  m_entries[bin]++;
  m_nentries++;
  m_sum += x;
  m_sum_sq += x * x;
  return bin;
}

void
Hist::save(const std::string& filename) const
{
  std::ofstream file;
  file.open(filename);
  file << m_steps << " " << m_low << " " << m_high << " " << std::endl;
  for (auto x : m_entries) {
    file << x << " ";
  }
  file << std::endl;
  file.close();
}

void
Hist::save(std::ofstream& filehandle) const
{
  filehandle << m_steps << " " << m_low << " " << m_high << " " << std::endl;
  for (auto x : m_entries) {
    filehandle << x << " ";
  }
  filehandle << std::endl;
}

bool
Hist::is_running()
{
  return true;
}

void
Hist::clean()
{
  m_sum = 0;
  m_sum_sq = 0;
  m_nentries = 0;
  for (auto& elem : m_entries) {
    elem = 0;
  }
  m_std_set = false;
  m_mean_set = false;
}

double
Hist::mean()
{
  if (m_mean_set)
    return m_mean;
  m_mean = m_sum / m_nentries;
  m_mean_set = true;
  return m_mean;
}

double
Hist::std()
{
  if (m_std_set)
    return m_std;
  m_mean = mean();
  m_std = sqrt((m_sum_sq + m_nentries * m_mean * m_mean - 2 * m_sum * m_mean) / (m_nentries - 1));
  m_std_set = true;
  return m_std;
}

} // namespace dunedaq::dqm

#endif // DQM_INCLUDE_DQM_HIST_HPP_
