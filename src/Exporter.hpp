/**
 * @file Exporter.hpp Exporter...
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_EXPORTER_HPP_
#define DQM_SRC_EXPORTER_HPP_

#include <librdkafka/rdkafkacpp.h>
#include <string>

namespace dunedaq::dqm
{

class KafkaStream
{
  public:
    KafkaStream(const std::string & param);
    void kafka_exporter(std::string input, std::string topic);
    RdKafka::Producer *m_producer;
};

KafkaStream::KafkaStream(const std::string &param)
{
  //Kafka server settings
  std::string brokers = param;
  std::string errstr;

  RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
  conf->set("bootstrap.servers", brokers, errstr);
  conf->set("client.id", "dqm", errstr);
  // conf->set("message.max.bytes", "1000000000", errstr);
  //Create producer instance
  m_producer = RdKafka::Producer::create(conf, errstr);
}

void KafkaStream::kafka_exporter(std::string input, std::string topic)
{
  RdKafka::ErrorCode err = m_producer->produce(
                      topic,
                      RdKafka::Topic::PARTITION_UA,
                      RdKafka::Producer::RK_MSG_COPY,
                      const_cast<char *>(input.c_str()), input.size(),
                      nullptr,
                      0,
                      0,
                      nullptr,
                      nullptr);

  if (err != RdKafka::ERR_NO_ERROR) {
     TLOG()<< "% Failed to produce to topic " << topic << ": " <<
           RdKafka::err2str(err);
  }
}

void
KafkaExport(std::string &kafka_address, std::string input, std::string topic)
{
  static dqm::KafkaStream stream(kafka_address);
  stream.kafka_exporter(input, topic);
}

} //namespace dqm

#endif // DQM_SRC_EXPORTER_HPP_
