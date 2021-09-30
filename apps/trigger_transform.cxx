// * This is part of the DUNE DAQ Application Framework, copyright 2020.
// * Licensing/copyright details are in the COPYING file that you should have received with this code.
#include <iostream>
#include <string>
#include <highfive/H5File.hpp>
#include <highfive/H5Object.hpp>
#include <librdkafka/rdkafkacpp.h>
#include "dataformats/TriggerRecord.hpp"
#include "dataformats/wib/WIBFrame.hpp"
#include <ers/StreamFactory.hpp>
#include "dqm/ChannelMapper.hpp"
#include <stdlib.h>     //for using the function sleep


namespace dunedaq
{ // namespace dunedaq

  ERS_DECLARE_ISSUE(triggertransform, CannotPostToDb,
                    "Cannot post to Influx DB : " << error,
                    ((std::string)error))

  ERS_DECLARE_ISSUE(triggertransform, CannotCreateConsumer,
                    "Cannot create consumer : " << fatal,
                    ((std::string)fatal))

  ERS_DECLARE_ISSUE(triggertransform, CannotConsumeMessage,
                    "Cannot consume message : " << error,
                    ((std::string)error))

  ERS_DECLARE_ISSUE(triggertransform, IncorrectParameters,
                    "Incorrect parameters : " << fatal,
                    ((std::string)fatal))

  ERS_DECLARE_ISSUE(kafkaraw, CannotProduce,
                    "Cannot produce to kafka " << error,
                    ((std::string)error))

  ERS_DECLARE_ISSUE(kafkaraw, WrongURI,
                    "Incorrect URI" << uri,
                    ((std::string)uri))
} // namespace dunedaq

RdKafka::Producer *m_producer;
std::string m_host;
std::string m_port;
std::string m_topic;
int apa_count;
int fragments_count;
int interval_of_capture = 100;

std::unique_ptr<swtpg::PdspChannelMapService> channelMap;
const char* readout_share_cstr = getenv("READOUT_SHARE");
std::string readout_share(readout_share_cstr);
std::string channel_map_rce = readout_share +  "/config/protoDUNETPCChannelMap_RCE_v4.txt";
std::string channel_map_felix = readout_share + "/config/protoDUNETPCChannelMap_FELIX_v4.txt";

void readDataset(std::string path_dataset, void *buff)
{

  std::string tr_header = "TriggerRecordHeader";
  if (path_dataset.find(tr_header) != std::string::npos)
  {
    std::cout << "--- TR header dataset" << path_dataset << std::endl;
    dunedaq::dataformats::TriggerRecordHeader trh(buff);
    std::cout << "Run number: " << trh.get_run_number()
              << " Trigger number: " << trh.get_trigger_number()
              << " Requested fragments: " << trh.get_num_requested_components() << std::endl
              << " Sequence : " << trh.get_sequence_number() << std::endl
              << " Max Swquence: " << trh.get_max_sequence_number() << std::endl;
    std::cout << "============================================================" << std::endl;

    sleep(60); //For the kafka broker not to be overhelmed... To improve

  }
  else
  {
    std::cout << "+++ Fragment dataset" << path_dataset << std::endl;
    dunedaq::dataformats::Fragment frag(buff, dunedaq::dataformats::Fragment::BufferAdoptionMode::kReadOnlyMode);
    // Here I can now look into the raw data
    // As an example, we print a couple of attributes of the Fragment header and then dump the first WIB frame.
    if (frag.get_fragment_type() == dunedaq::dataformats::FragmentType::kTPCData)
    {
      std::cout << "Fragment with Run number: " << frag.get_run_number()
                << " Trigger number: " << frag.get_trigger_number()
                << " APA: " << std::to_string(apa_count)
                << " GeoID: " << frag.get_element_id() << std::endl;

      // Get pointer to the first WIB frame
      auto wfptr = reinterpret_cast<dunedaq::dataformats::WIBFrame *>(frag.get_data());
      size_t raw_data_packets = (frag.get_size() - sizeof(dunedaq::dataformats::FragmentHeader)) / sizeof(dunedaq::dataformats::WIBFrame);

      //Message to be sent by kafka
      //Sends first informations about the trigger record and then about the WIB frame sent
      std::string message_to_kafka;
      std::cout << "Fragment contains " << raw_data_packets << " WIB frames"
                << " Total fragments : " << std::to_string(fragments_count) << std::endl;
      for (size_t i = 0; i < raw_data_packets; i += interval_of_capture)
      {
        message_to_kafka = std::to_string(apa_count) + ";" + std::to_string(fragments_count) + ";" + std::to_string(raw_data_packets/interval_of_capture) + ";" + std::to_string(frag.get_run_number()) + ";" + std::to_string(frag.get_trigger_number()) + ";" + std::to_string(frag.get_element_id().region_id) + ";" + std::to_string(frag.get_element_id().element_id) + ";";

        auto wf1ptr = reinterpret_cast<dunedaq::dataformats::WIBFrame *>(frag.get_data() + i * sizeof(dunedaq::dataformats::WIBFrame));

        //Adds wib frame id
        message_to_kafka += std::to_string(i/interval_of_capture) + "\n";

        for (int j = 0; j < 256; j++)
        {
          unsigned int offline = dunedaq::dqm::getOfflineChannel(*channelMap, wf1ptr, j);
          unsigned int channel_coordinate = dunedaq::dqm::LocalWireNumber(*channelMap, offline);
          unsigned int plane = dunedaq::dqm::GetPlane(*channelMap, offline);
          //Adds plane and channel position
          
          message_to_kafka += std::to_string(plane) + " " + std::to_string(channel_coordinate)+ "\n";

          message_to_kafka += std::to_string(wfptr->get_channel(j)) + "\n";

          //std::cout << "Channel " << std::to_string(j) << " : " << wfptr->get_channel(j) << std::endl;
        }
        //std::cout << message_to_kafka << std::endl;

        /*
            // print first WIB header
            if (i==0) {
                  std::cout << "First WIB header:"<< *(wfptr->get_wib_header());
                  //std::cout << "Printout sampled timestamps in WIB headers: " ;
                  //std::cout << "Channels :"<< *(wfptr) << std::endl;
                  for(int j = 0; j< 256; j++)
                  {
                    std::cout << "Channel " << std::to_string(j) << " : " << wfptr->get_channel(j) << std::endl;
                  }
            }*/
        // printout timestamp every now and then, only as example of accessing data...
        //if(i%1000 == 0) std::cout << "Timestamp " << i << ": " << wf1ptr->get_timestamp() << " ";
        //kafka_exporter(message, "dunedqm-incommingchannel2");

        try
        {
          // serialize it to BSON
          RdKafka::ErrorCode err = m_producer->produce(m_topic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY, const_cast<char *>(message_to_kafka.c_str()), message_to_kafka.size(), nullptr, 0, 0, nullptr, nullptr);
          //std::cout << message_to_kafka << std::endl;
          //if (err != RdKafka::ERR_NO_ERROR) { dunedaq::kafkaraw::CannotProduce(ERS_HERE, "% Failed to produce " + RdKafka::err2str(err));}
          if (err != RdKafka::ERR_NO_ERROR)
          {
            std::cout << "% Failed to produce " + RdKafka::err2str(err);
          }
          else
          {
            //std::cout << "Frame sent : " << message_to_kafka << std::endl;
            sleep(0.1); //For the kafka broker not to be overhelmed... To improve
          }
        }
        catch (const std::exception &e)
        {
          std::string s = e.what();
          //std::cout << s << std::endl;
          //ers::error(dunedaq::kafkaraw::CannotProduce(ERS_HERE, "Error [" + s + "] message(s) were not delivered"));
        }
      }

      sleep(1); //For the kafka broker not to be overhelmed... To improve

      std::cout << std::endl;
    }
    else
    {
      std::cout << "Skipping unknown fragment type" << std::endl;
    }
  }
}

// Recursive function to traverse the HDF5 file
void exploreSubGroup(HighFive::Group parent_group, std::string relative_path, std::vector<std::string> &path_list)
{
  int link_count = 0;
  bool is_link = false;
  std::vector<std::string> childNames = parent_group.listObjectNames();
  for (auto &child_name : childNames)
  {
    //std::cout << "Group: " << child_name << std::endl;

    std::string full_path = relative_path + "/" + child_name;
    HighFive::ObjectType child_type = parent_group.getObjectType(child_name);
    if (child_type == HighFive::ObjectType::Dataset)
    {
      if(child_name.find("TriggerRecordHeader"))
      {
        link_count++;
        is_link = true;
      }
      
      //std::cout << "Dataset IN IF : " << child_name << std::endl;
      //fragments_count = parent_group.listObjectNames().size();
      path_list.push_back(full_path);
    }
    else if (child_type == HighFive::ObjectType::Group)
    {
      //std::cout << "Group: " << child_name << std::endl;
      //std::cout << "VECTOR LENGTH : " << std::to_string(parent_group.listObjectNames().size()) << std::endl;
      apa_count = parent_group.listObjectNames().size();

      //std::cout << std::to_string(apa_count) << std::endl;

      HighFive::Group child_group = parent_group.getGroup(child_name);
      // start the recusion
      std::string new_path = relative_path + "/" + child_name;
      exploreSubGroup(child_group, new_path, path_list);
    }
  }
  if (is_link) {fragments_count = link_count;}
}

std::vector<std::string> traverseFile(HighFive::File input_file, int num_trs)
{

  // Vector containing the path list to the HDF5 datasets
  std::vector<std::string> path_list;

  std::string top_level_group_name = input_file.getPath();
  if (input_file.getObjectType(top_level_group_name) == HighFive::ObjectType::Group)
  {
    HighFive::Group parent_group = input_file.getGroup(top_level_group_name);
    exploreSubGroup(parent_group, top_level_group_name, path_list);
  }
  // =====================================
  // THIS PART IS FOR TESTING ONLY
  // FIND A WAY TO USE THE HDF5DAtaStore
  int i = 0;
  std::string prev_ds_name;
  for (auto &dataset_path : path_list)
  {
    if (dataset_path.find("Fragment") == std::string::npos && prev_ds_name.find("TriggerRecordHeader") != std::string::npos && i >= num_trs)
    {
      break;
    }
    if (dataset_path.find("TriggerRecordHeader") != std::string::npos)
      ++i;

    //readDataset(dataset_path);
    HighFive::Group parent_group = input_file.getGroup(top_level_group_name);
    HighFive::DataSet data_set = parent_group.getDataSet(dataset_path);
    HighFive::DataSpace thedataSpace = data_set.getSpace();
    size_t data_size = data_set.getStorageSize();
    char *membuffer = new char[data_size];
    data_set.read(membuffer);
    readDataset(dataset_path, membuffer);
    delete[] membuffer;

    prev_ds_name = dataset_path;
  }
  // =====================================

  return path_list;
}

int main(int argc, char **argv)
{
  int num_trs = 1000;
  channelMap.reset(new swtpg::PdspChannelMapService(channel_map_rce, channel_map_felix));
  /*
  if(argc <2) {
    std::cerr << "Usage: data_file_browser <fully qualified file name> [number of events to read]" << std::endl;
    return -1;
  }

  if(argc == 3) {
    num_trs = std::stoi(argv[2]);
  }   
  // Open the existing hdf5 file
  HighFive::File file(argv[1], HighFive::File::ReadOnly);*/
  m_host = "188.185.122.48";
  m_port = "9092";
  m_topic = "dunedqm-incommingchannel2";
  //Kafka server settings
  std::string brokers = m_host + ":" + m_port;
  std::string errstr;

  RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
  conf->set("bootstrap.servers", brokers, errstr);
  if (errstr != "")
  {
    dunedaq::kafkaraw::CannotProduce(ERS_HERE, "Bootstrap server error : " + errstr);
  }
  if (const char *env_p = std::getenv("DUNEDAQ_APPLICATION_NAME"))
    conf->set("client.id", env_p, errstr);
  else
    conf->set("client.id", "rawdataProducerdefault", errstr);

  if (errstr != "")
  {
    dunedaq::kafkaraw::CannotProduce(ERS_HERE, "Producer configuration error : " + errstr);
  }
  //Create producer instance
  m_producer = RdKafka::Producer::create(conf, errstr);

  if (errstr != "")
  {
    dunedaq::kafkaraw::CannotProduce(ERS_HERE, "Producer creation error : " + errstr);
  }

  HighFive::File file("/eos/home-y/yadonon/swtest_run000004_0000_glehmann_20210924T141346.hdf5", HighFive::File::ReadOnly);

  std::vector<std::string> data_path = traverseFile(file, num_trs);
  return 0;
}
