/**
 * @file DQMIssues.hpp DQM system related ERS issues
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_INCLUDE_DQM_DQMISSUES_HPP_
#define DQM_INCLUDE_DQM_DQMISSUES_HPP_

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

} // namespace dunedaq

#endif // DQM_INCLUDE_DQM_DQMISSUES_HPP_
