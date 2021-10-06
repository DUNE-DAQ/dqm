/**
 * @file Fourier.hpp Fast fourier transform using the Cooley-Tukey algorithm
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_FOURIER_HPP_
#define DQM_SRC_FOURIER_HPP_

// dqm
#include "AnalysisModule.hpp"
#include "Decoder.hpp"

#include "dataformats/TriggerRecord.hpp"
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

  Fourier(double inc, int npoints);

  void fill(double value);
  CArray compute_fourier();
  CArray compute_fourier_normalized();
  CArray compute_fourier_def();
  std::vector<double> get_frequencies();

  void clean();

  // void save(const std::string& filename) const;
  // void save(std::ofstream& filehandle) const;

  // void save_fourier(const std::string& filename) const;
  // void save_fourier(std::ofstream& filehandle) const;
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
  //Compute mean in order to centre time series around 0 in y 
  double mean = 0;
  for (size_t i = 0; i < input.size(); i++) mean += input[i];
  mean = (double) mean/(input.size());
  
  CArray output(input.size());
  for (size_t i = 0; i < input.size(); i++) {
    output[i] = input[i] - mean;
    //if (i < 100)
    //  std::cout << "Prep " << output[i] << " " << input[i] << std::endl;
  }
  return output;
}

void
Fourier::fast_fourier_transform(CArray& x)
{
    const size_t N = x.size();
    if (N <= 1) return;

    // divide
    CArray even = x[std::slice(0, N/2, 2)];
    CArray  odd = x[std::slice(1, N/2, 2)];

    // conquer
    fast_fourier_transform(even);
    fast_fourier_transform(odd);

    // combine
    for (size_t k = 0; k < N/2; ++k)
    {
        Complex t = std::polar(1.0, -2 * M_PI * k / N) * odd[k];
        x[k    ] = even[k] + t;
        x[k+N/2] = even[k] - t;
    }
}

CArray
Fourier::compute_fourier()
{
  CArray input = fourier_prep(m_data);
  std::cout << "Input prepared" << std::endl;
  fast_fourier_transform(input);
  std::cout << "Transform performed" << std::endl;
  return input;
}

CArray
Fourier::compute_fourier_def()
{
  if (m_data.size() != m_npoints)
    TLOG() << "The number of points in the data is different from the number of points specified";
  CArray output = CArray(0.0, m_data.size() / 2);

  // Only until m_data.size() / 2, the others correspond to the negative frequencies
  for (size_t k = 0; k < m_data.size() / 2; ++k)
    for (size_t m = 0; m < m_data.size(); ++m) {
      output[k] += m_data[m] * std::exp( Complex(0, -2) * M_PI * static_cast<double>(k) * static_cast<double>(m) / static_cast<double>(m_data.size()));
    }
  return output;
}

CArray
Fourier::compute_fourier_normalized()
{
  CArray output = compute_fourier_def();

  // Only until m_data.size() / 2, the others correspond to the negative frequencies
  for (size_t k = 0; k < m_data.size() / 2; ++k)
    output[k] = 2. / m_npoints * abs(output[k]);
  return output;
}


std::vector<double>
Fourier::get_frequencies()
{
  std::vector<double> ret;
  for (int i = 0; i < m_npoints/2; ++i)
    ret.push_back(i / (m_inc_size * m_npoints));
  // Don't return the negative ones
  // for (int i = -m_npoints/2; i < 0; ++i)
  //   ret.push_back(i / (m_inc_size * m_npoints));
  return ret;
}

void
Fourier::fill(double value) // NOLINT(build/unsigned)
{
  if (m_data.size() < m_npoints) m_data.push_back(value);
  //else TLOG() << "Cannot fill time series beyond declared size. Check that # of frames requested in config is consistent with declared length." << std::endl; 
}

void
Fourier::clean()
{
  m_data.clear();
}

// void
// Fourier::save(const std::string& filename) const
// {
//   std::ofstream file;
//   file.open(filename);
//   file << m_start << " " << m_end << " " << m_inc_size << " " << m_npoints << " " << std::endl;
//   for (auto x : m_data) {
//     file << x << " ";
//   }
//   file << std::endl;
// }

// void
// Fourier::save(std::ofstream& filehandle) const
// {
//   filehandle << m_start << " " << m_end << " " << m_inc_size << " " << m_npoints << " " << std::endl;
//   for (auto x : m_data) {
//     filehandle << x << " ";
//   }
//   filehandle << std::endl;
// }

// void
// Fourier::save_fourier(const std::string& filename) const
// {
//   std::ofstream file;
//   file.open(filename);
//   file << m_start << " " << m_end << " " << m_inc_size << " " << m_npoints << " " << m_rebin_factor << std::endl;
//   for (auto x : m_fourier_transform) {
//     file << x << " ";
//   }
//   file << std::endl;
// }

// void
// Fourier::save_fourier(std::ofstream& filehandle) const
// {
//   filehandle << m_start << " " << m_end << " " << m_inc_size << " " << m_npoints << " " << m_rebin_factor << std::endl;
//   for (auto x : m_fourier_transform) {
//     filehandle << x << " ";
//   }
//   filehandle << std::endl;
// }


} // namespace dunedaq::dqm

#endif // DQM_SRC_FOURIER_HPP_
