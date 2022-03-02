/**
 * @file Fourier.hpp Fast fourier transform using the Cooley-Tukey algorithm
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_INCLUDE_DQM_FOURIER_HPP_
#define DQM_INCLUDE_DQM_FOURIER_HPP_

// dqm
#include "AnalysisModule.hpp"
#include "Decoder.hpp"

#include "daqdataformats/TriggerRecord.hpp"
#include "logging/Logging.hpp"

#include <complex>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <valarray>
#include <vector>

namespace dunedaq::dqm {

typedef std::complex<double> Complex;
typedef std::valarray<Complex> CArray;

class Fourier
{
public:
  CArray fourier_prep(const std::vector<double>& input) const;
  void fast_fourier_transform(CArray& x);

  double m_inc_size;
  int m_npoints;
  std::vector<double> m_data;
  std::vector<double> m_transform;

  Fourier(double inc, int npoints);

  void fill(double value);
  CArray compute_fourier();
  void compute_fourier_normalized();
  CArray compute_fourier_def();
  std::vector<double> get_frequencies();
  void clean();

  double get_transform(int index);
};

Fourier::Fourier(double inc, int npoints) // NOLINT(build/unsigned)
  : m_inc_size(inc)
  , m_npoints(npoints)
{
  m_data.reserve(m_npoints);
}

CArray
Fourier::fourier_prep(const std::vector<double>& input) const
{
  CArray output(input.size());
  for (size_t i = 0; i < input.size(); i++) {
    output[i] = input[i];
    if (i < 100)
      TLOG() << "Prep " << output[i] << " " << input[i];
  }
  return output;
}

CArray
Fourier::compute_fourier_def()
{
  // if (m_data.size() != m_npoints)
  //  TLOG() << "The number of points in the data is different from the number of points specified, m_data.size() = " <<
  //  m_data.size() << " and m_npoints = " << m_npoints;
  CArray output = CArray(0.0, m_data.size() / 2);

  // Only until m_data.size() / 2, the others correspond to the negative frequencies
  for (size_t k = 0; k < m_data.size() / 2; ++k)
    for (size_t m = 0; m < m_data.size(); ++m) {
      output[k] += m_data[m] * std::exp(Complex(0, -2) * M_PI * static_cast<double>(k) * static_cast<double>(m) /
                                        static_cast<double>(m_data.size()));
    }
  return output;
}

void
Fourier::compute_fourier_normalized()
{
  CArray output = compute_fourier_def();

  // Only until m_data.size() / 2, the others correspond to the negative frequencies
  for (size_t k = 0; k < m_data.size() / 2; ++k) {
    output[k] = 2. / m_npoints * abs(output[k]);
    m_transform.push_back(abs(output[k]));
  }
}

double
Fourier::get_transform(int index)
{
  if (index < 0 || static_cast<size_t>(index) >= m_transform.size()) {
    TLOG() << "WARNING: Fourier::get_transform called with index out of range, index=" << index
           << ", size of m_transform vector is " << m_transform.size();
    return 0.0;
  }
  return m_transform[index];
}

std::vector<double>
Fourier::get_frequencies()
{
  std::vector<double> ret;
  for (int i = 0; i < m_npoints / 2; ++i)
    ret.push_back(i / (m_inc_size * m_npoints));
  // Don't return the negative ones
  // for (int i = -m_npoints/2; i < 0; ++i)
  //   ret.push_back(i / (m_inc_size * m_npoints));
  return ret;
}

void
Fourier::fill(double value) // NOLINT(build/unsigned)
{
  m_data.push_back(value);
}

void
Fourier::clean()
{
  m_data.clear();
  m_transform.clear();
}

} // namespace dunedaq::dqm

#endif // DQM_INCLUDE_DQM_FOURIER_HPP_
