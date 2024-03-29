/**
 * @file Fourier.hpp Fast fourier transforms using the fftw3 library
 *
 * This is part of the DUNE DAQ, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_DQM_ALGS_FOURIER_CPP_
#define DQM_SRC_DQM_ALGS_FOURIER_CPP_

// dqm

#include "dqm/algs/Fourier.hpp"

#include <complex>
#include <string>
#include <valarray>
#include <vector>

// #include <complex> has to be before this include
#include <fftw3.h>

namespace dunedaq::dqm {

Fourier::Fourier(double inc, int npoints) // NOLINT(build/unsigned)
  : m_inc_size(inc)
  , m_npoints(npoints)
{
  m_data.reserve(npoints);
  m_transform = std::vector<std::complex<double>> (npoints);
}


/**
 * @brief Compute the absolute value of the fourier transform
 *        using the FFTW library
 */
void
Fourier::compute_fourier_transform() {

	
  if (m_data.size() < (size_t)m_npoints) {
    //m_npoints = m_data.size();
    ers::info(ParameterChange(ERS_HERE, "Input doesn't have the expected min n_samples for the Fourier transform, " + std::to_string(m_data.size()) + " instead of "+ std::to_string(m_npoints) + ". Skipping this event."));
    return;
  }

//  if (m_transform.size() != (size_t)m_npoints) {
//    m_transform.resize(m_npoints);
//  }

  std::vector<double> tmp(m_npoints);

  // A plan is created, executed and destroyed each time_t
  // Not the most efficient way but using the new-array interface
  // and creating a single plan that is passed around crashes for
  // an unknown reason. Anyway in the docs they say that creating a new plan
  // once another one has been created before for the same size is cheap
  // FFTW_MEASURE instead of FFTW_ESTIMATE doesn't change the output
  fftw_plan plan = fftw_plan_r2r_1d(m_npoints, m_data.data(), tmp.data(), FFTW_R2HC, FFTW_ESTIMATE );
  if (plan == NULL) {
    ers::error(CouldNotCreateFourierPlan(ERS_HERE, ""));
    return;
  }
  fftw_execute(plan);
  fftw_destroy_plan(plan);
  // After the transform is computed half of the elements of the
  // output array are the real part and the other half are the
  // complex part

  // Caveats, i = 0 and i = m_npoints/2 are already real and i = 0 is already
  // positive so only i = m_npoints/2 has to be changed
  for (int i = 1; i < m_npoints / 2; ++i) {
    m_transform[i] = {tmp[i], tmp[m_npoints - i]};
  }
  m_transform[0] = {tmp[0], 0};
  m_transform[m_npoints / 2] = {tmp[m_npoints / 2], 0};
  m_transform.resize(m_npoints / 2 + 1);
}


std::vector<std::complex<double>>
Fourier::get_transform() {
  return m_transform;
}

/**
 * @brief Get the absolute value of the fourier transform at index index
 *
 *        Must be called after compute_fourier_transform
 */
std::complex<double>
Fourier::get_transform_at(int index)
{
  if (index < 0 || static_cast<size_t>(index) >= m_transform.size()) {
    TLOG() << "WARNING: Fourier::get_transform called with index out of range, index=" << index
           << ", size of m_transform vector is " << m_transform.size();
    return 0.0;
  }
  return m_transform[index];
}

/**
 * @brief Get the output frequencies (only non-negative frequencies)
 */
std::vector<double>
Fourier::get_frequencies()
{
  std::vector<double> ret;
  for (int i = 0; i <= m_npoints / 2; ++i)
    ret.push_back(i / (m_inc_size * m_npoints));
  // Don't return the negative frequencies by commenting the following block
  // for (int i = -m_npoints/2; i < 0; ++i)
  //   ret.push_back(i / (m_inc_size * m_npoints));
  return ret;
}

/**
 * @brief Fill the values that will be used to compute the fourier transform
 *
 */
void
Fourier::fill(double value) // NOLINT(build/unsigned)
{
  m_data.push_back(value);
}

/**
 * @brief Clear the vectors that hold the data and the fourier transform
 *
 *        This function leaves the Fourier object not usable anymore
 */
void
Fourier::clean()
{
  m_data.clear();
  m_transform.clear();
}

} // namespace dunedaq::dqm

#endif // DQM_SRC_DQM_ALGS_FOURIER_CPP_
