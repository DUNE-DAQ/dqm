/**
 * @file Issues.hpp DQM system related ERS issues
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_INCLUDE_DQM_ISSUES_HPP_
#define DQM_INCLUDE_DQM_ISSUES_HPP_

#include "dfmessages/Types.hpp"

#include <ers/Issue.hpp>

#include <string>

namespace dunedaq {

ERS_DECLARE_ISSUE(dqm, ChannelMapError, "Channel Map parameter not valid:" << error, ((std::string)error))

ERS_DECLARE_ISSUE(dqm,
                  InvalidEnvVariable,
                  "Trying to get the env variable " << env << " did not work",
                  ((std::string)env))

ERS_DECLARE_ISSUE(dqm, CouldNotOpenFile, "Unable to open the file " << file, ((std::string)file))

ERS_DECLARE_ISSUE(dqm,
                  InvalidTimestamp,
                  "The timestamp " << timestamp << " is not valid",
                  ((dfmessages::timestamp_t)timestamp))

ERS_DECLARE_ISSUE(dqm, ProcessorError, "Error in DQMProcessor: " << error, ((std::string)error))

ERS_DECLARE_ISSUE(dqm, InvalidData, "Data was not valid: " << error, ((std::string)error))

ERS_DECLARE_ISSUE(dqm, CouldNotCreateFourierPlan, "Fourier plan was not created ", ((std::string)error))

ERS_DECLARE_ISSUE(dqm, FrontEndNotSupported, "Frontend is not supported: ", ((std::string)frontend_type))

ERS_DECLARE_ISSUE(dqm, EmptyFragments, "Empty fragments received", ((std::string)fragment_info))

ERS_DECLARE_ISSUE(dqm, EmptyData, "Data is empty after removing empty fragments", ((std::string)empty))

ERS_DECLARE_ISSUE(dqm, TimestampsNotAligned, "Timestamps are not aligned, DQM will align them", ((std::string)empty))

ERS_DECLARE_ISSUE(dqm, InvalidInput, "Invalid input: " << why, ((std::string)why))

ERS_DECLARE_ISSUE(dqm, BadCrateSlotFiber,
                  "Bad crate, slot, fiber: (" << crate << ", " << slot << ", " << fiber << ")",
                  ((int)crate)((int)slot)((int)fiber))

ERS_DECLARE_ISSUE(dqm, ParameterChange, "Parameters changed: " << why, ((std::string)why))

ERS_DECLARE_ISSUE(dqm, ModuleNotImported, "Python module " << module << " couldn't be imported", ((std::string)module))

ERS_DECLARE_ISSUE(dqm, PythonModuleCrashed, "Python module " << module << " crashed", ((std::string)module))

ERS_DECLARE_ISSUE(dqm, MissingConnection, "The " << conn_desc << " connection is not available, and this will prevent this module from working correctly. This problem is probably the result of a mistake in the connection information given to this module at INIT time, please check that information for possible problems.", ((std::string)conn_desc))

} // namespace dunedaq

#endif // DQM_INCLUDE_DQM_ISSUES_HPP_
