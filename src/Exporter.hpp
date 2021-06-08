#pragma once

#include <iostream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <cstring>

#if _AIX
#include <unistd.h>
#endif

#include <librdkafka/rdkafkacpp.h>

static volatile sig_atomic_t run = 1;

static void sigterm (int sig) 
{
  run = 0;
}

class ExampleDeliveryReportCb : public RdKafka::DeliveryReportCb 
{
  public:
  void dr_cb (RdKafka::Message &message) 
  {
    //If message.err() is non-zero the message delivery failed permanently for the message.
    if (message.err())
      std::cerr << "% Message delivery failed: " << message.errstr() << std::endl;
    else
      std::cerr << "% Message delivered to topic " << message.topic_name() <<
        " [" << message.partition() << "] at offset " <<
        message.offset() << std::endl;
  }
};

void KafkaExport(std::string input, std::string topic)
{
  std::cerr << "Exporting" << std::endl;
  std::string brokers = "dbnile-kafka-a-5.cern.ch:9443";
  //std::string topic   = "testdunedqm";

  char hostname[128];
  std::string errstr;
  
  RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

  if (conf->set("bootstrap.servers", brokers, errstr) != RdKafka::Conf::CONF_OK) 
  {
    std::cerr << errstr << std::endl;
    exit(1);
  }

  if (conf->set("client.id", "myid", errstr) != RdKafka::Conf::CONF_OK)
  {
    std::cerr << errstr << std::endl;
    exit(1);
  }

  //Pip's additions - security & certificate config
  //--------------------------------------------------
  if (conf->set("security.protocol", "SSL", errstr) != RdKafka::Conf::CONF_OK)
  {
    std::cerr << errstr << std::endl;
    exit(1);
  }

  if (conf->set("ssl.ca.location", "/afs/cern.ch/user/p/pahamilt/certs/cerncacerts.pem", errstr) != RdKafka::Conf::CONF_OK)
  {
    std::cerr << errstr << std::endl;
    exit(1);
  }

  if (conf->set("ssl.certificate.location", "/afs/cern.ch/user/p/pahamilt/certs/myCertificate.crt.pem", errstr) != RdKafka::Conf::CONF_OK)
  {
    std::cerr << errstr << std::endl;
    exit(1);
  }

  if (conf->set("ssl.key.location", "/afs/cern.ch/user/p/pahamilt/certs/myCertificate.key.pem", errstr) != RdKafka::Conf::CONF_OK)
  {
    std::cerr << errstr << std::endl;
    exit(1);
  }
  //------------------------------------------------
  
  //Bit I don't understand
  signal(SIGINT, sigterm);
  signal(SIGTERM, sigterm);

  ExampleDeliveryReportCb ex_dr_cb;
  if (conf->set("dr_cb", &ex_dr_cb, errstr) != RdKafka::Conf::CONF_OK) {
    std::cerr << errstr << std::endl;
    exit(1);
  }

  //Create producer instance
  RdKafka::Producer *producer = RdKafka::Producer::create(conf, errstr);
  if (!producer)
  {
    std::cerr << "Failed to create producer: " << errstr << std::endl;
    exit(1);
  }
  delete conf;

  /*
  //TEXT ENTRY - REPLACE THIS
  //--------------------------
  std::cout << "% Type message value and hit enter " << "to produce message." << std::endl;

  for (std::string line; run && std::getline(std::cin, line);) 
  {
    if (line.empty()) 
    {
      producer->poll(0);
      continue;
    }
  //--------------------------
  */

  char * input_bytes = const_cast<char *>(input.c_str());

  retry:
    //RdKafka::ErrorCode err = producer->produce(topic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY, const_cast<char *>(line.c_str()), line.size(), NULL, 0, 0, NULL, NULL);
    RdKafka::ErrorCode err = producer->produce(topic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY, input_bytes, input.size(), NULL, 0, 0, NULL, NULL);

    if (err != RdKafka::ERR_NO_ERROR) 
    {
      std::cerr << "% Failed to produce to topic " << topic << ": " << RdKafka::err2str(err) << std::endl;

      if (err == RdKafka::ERR__QUEUE_FULL) 
      {
        producer->poll(1000/*block for max 1000ms*/);
        goto retry;
      }

    } 
    else 
    {
      //std::cerr << "% Enqueued message (" << line.size() << " bytes) " << "for topic " << topic << std::endl;
      std::cerr << "% Enqueued message (" << input.size() << " bytes) " << "for topic " << topic << std::endl;
    }

    producer->poll(0);
  //}

  std::cerr << "% Flushing final messages..." << std::endl;
  producer->flush(10*1000 );

  if (producer->outq_len() > 0) std::cerr << "% " << producer->outq_len() << " message(s) were not delivered" << std::endl;

  delete producer;

  //return 0;
} 
