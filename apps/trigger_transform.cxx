// * This is part of the DUNE DAQ Application Framework, copyright 2020.
// * Licensing/copyright details are in the COPYING file that you should have received with this code.
#include <iostream>
#include <string>
#include <highfive/H5File.hpp>
#include <highfive/H5Object.hpp>
#include <librdkafka/rdkafkacpp.h>
#include <ers/StreamFactory.hpp>
#include "daqdataformats/TriggerRecord.hpp"
#include "detdataformats/wib/WIBFrame.hpp"
#include "boost/program_options.hpp"
#include "dqm/ChannelMapper.hpp"
#include <stdlib.h>     //for using the function sleep
#include <dirent.h>
#include <fstream>

namespace bpo = boost::program_options;


namespace dunedaq
{ // namespace dunedaq

  ERS_DECLARE_ISSUE(triggertransform, CannotOpenFile,
                    "Cannot open file : " << error,
                    ((std::string)error))

  ERS_DECLARE_ISSUE(triggertransform, ErrorReadingFile,
                    "Error reading : " << error,
                    ((std::string)error))

  ERS_DECLARE_ISSUE(triggertransform, CannotOpenFolder,
                    "Cannot open folder : " << error,
                    ((std::string)error))

  ERS_DECLARE_ISSUE(triggertransform, IncorrectParameters,
                    "Incorrect parameters : " << fatal,
                    ((std::string)fatal))

  ERS_DECLARE_ISSUE(triggertransform, CannotProduce,
                    "Cannot produce to kafka " << error,
                    ((std::string)error))
} // namespace dunedaq

RdKafka::Producer *m_producer;
std::string topic;
int apa_count;
int fragments_count;
int interval_of_capture;

std::unique_ptr<swtpg::PdspChannelMapService> channelMap;
const char* readout_share_cstr = getenv("READOUT_SHARE");
std::string readout_share(readout_share_cstr);
std::string channel_map_rce;
std::string channel_map_felix;
std::string data_Source_Name;

void readDataset(std::string path_dataset, void *buff)
{
  std::string tr_header = "TriggerRecordHeader";
  if (path_dataset.find(tr_header) == std::string::npos)
  {
    dunedaq::daqdataformats::Fragment frag(buff, dunedaq::daqdataformats::Fragment::BufferAdoptionMode::kReadOnlyMode);
    // Here I can now look into the raw data
    // As an example, we print a couple of attributes of the Fragment header and then dump the first WIB frame.
    if (frag.get_fragment_type() == dunedaq::daqdataformats::FragmentType::kTPCData)
    {

      // Get pointer to the first WIB frame
      //auto wfptr = reinterpret_cast<dunedaq::detdataformats::wib::WIBFrame *>(frag.get_data());
      size_t raw_data_packets = (frag.get_size() - sizeof(dunedaq::daqdataformats::FragmentHeader)) / sizeof(dunedaq::detdataformats::wib::WIBFrame);

      //Message to be sent by kafka
      //Sends first informations about the trigger record and then about the WIB frame sent
      std::string message_to_kafka;

      for (size_t i = 0; i < raw_data_packets; i += interval_of_capture)
      {
        message_to_kafka = std::to_string(apa_count) + ";" + std::to_string(fragments_count) + ";" + std::to_string(interval_of_capture) + ";" + std::to_string(frag.get_run_number()) + ";" + std::to_string(frag.get_trigger_number()) + ";" + std::to_string(frag.get_element_id().region_id) + ";" + std::to_string(frag.get_element_id().element_id) + ";" + std::to_string(raw_data_packets) + ";" + data_Source_Name + ";";


        auto wfptr_i = reinterpret_cast<dunedaq::detdataformats::wib::WIBFrame *>(static_cast<char*>(frag.get_data()) + i * sizeof(dunedaq::detdataformats::wib::WIBFrame));
        //auto wfptr_i = reinterpret_cast<dunedaq::detdataformats::wib::WIBFrame *>(i * sizeof(dunedaq::detdataformats::wib::WIBFrame));

        //auto wfptr_i = reinterpret_cast<dunedaq::detdataformats::wib::WIBFrame*>(frag.get_data());       

        //Adds wib frame id
        message_to_kafka += std::to_string(i/interval_of_capture) + "\n";

        for (int j = 0; j < 256; j++)
        {
          unsigned int offline = dunedaq::dqm::getOfflineChannel(*channelMap, wfptr_i, j);
          unsigned int channel_coordinate = dunedaq::dqm::LocalWireNumber(*channelMap, offline);
          unsigned int plane = dunedaq::dqm::GetPlane(*channelMap, offline);
          //Adds plane and channel position
          message_to_kafka += std::to_string(plane) + " " + std::to_string(channel_coordinate)+ "\n";
          message_to_kafka += std::to_string(wfptr_i->get_channel(j)) + "\n";

        }
        try
        {
          // serialize it to BSON
           
          RdKafka::ErrorCode err = m_producer->produce(topic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY, const_cast<char *>(message_to_kafka.c_str()), message_to_kafka.size(), nullptr, 0, 0, nullptr, nullptr);        

          if (err != RdKafka::ERR_NO_ERROR)
          {
            ers::error(dunedaq::triggertransform::CannotProduce(ERS_HERE, "% Failed to produce " + RdKafka::err2str(err)));
          }
          else
          {
            //std::cout << "Frame sent : " << message_to_kafka << std::endl;
            sleep(0.02); //For the kafka broker not to be overhelmed... To improve
          }
        }
        catch (const std::exception &e)
        {
          std::string s = e.what();
          //std::cout << s << std::endl;
          ers::error(dunedaq::triggertransform::CannotProduce(ERS_HERE, "Error [" + s + "] message(s) were not delivered"));
        }
      }

    }
    else
    {
      ers::error(dunedaq::triggertransform::CannotProduce(ERS_HERE, "unknown fragment type"));
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
        is_link = true;
        link_count++;
      }
      
      //std::cout << "Dataset IN IF : " << child_name << std::endl;
      //fragments_count = parent_group.listObjectNames().size();
      path_list.push_back(full_path);
    }
    else if (child_type == HighFive::ObjectType::Group)
    {
      apa_count = parent_group.listObjectNames().size();

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

  std::string broker;
  std::string string_folder = "";
  std::string string_interval_of_capture;

  char *folder = const_cast<char*>(string_folder.c_str());
  //char *folder = "/eos/home-y/yadonon/TriggerRecords/";
  //interval_of_capture = 100;



  bpo::options_description desc{"example: --rcemap /config/protoDUNETPCChannelMap_RCE_v4.txt --felixmap /config/protoDUNETPCChannelMap_FELIX_v4.txt --broker 127.0.0.1:9092 --Source defaultSource --topic dunedqm-incomingadcfrequency --folder /eos/home-y/yadonon/TriggerRecords/ --interval 100"};

  desc.add_options()
    ("help,h", "Help screen")
    ("broker,b", bpo::value<std::string>()->default_value("127.0.0.1:9092"), "Broker")
    ("source,s", bpo::value<std::string>()->default_value("defaultSource"), "Source")
    ("rcemap,s", bpo::value<std::string>()->default_value("/config/protoDUNETPCChannelMap_RCE_v4.txt"), "RCE channels map")
    ("felixmap,s", bpo::value<std::string>()->default_value("/config/protoDUNETPCChannelMap_FELIX_v4.txt"), "FELIX channels map")
    ("topic,t", bpo::value<std::string>()->default_value("dunedqm-incomingadcfrequency"), "Topic")
    ("folder,f", bpo::value<std::string>()->default_value("/eos/home-y/yadonon/TriggerRecords/"), "Folder")
    ("interval,i", bpo::value<std::string>()->default_value("100"), "Inverval of capture");



  bpo::variables_map vm;
    
  try 
  {
    auto parsed = bpo::command_line_parser(argc, argv).options(desc).run();
    bpo::store(parsed, vm);
  }
  catch (bpo::error const& e) 
  {
    ers::error(dunedaq::triggertransform::IncorrectParameters(ERS_HERE, e.what()));
  }

  if (vm.count("help")) {
    TLOG() << desc << std::endl;
    return 0;
  }
  
  try 
  {
    broker = vm["broker"].as<std::string>();     
    topic = vm["topic"].as<std::string>();
    string_folder = vm["folder"].as<std::string>();
    string_interval_of_capture = vm["interval"].as<std::string>();
    data_Source_Name = vm["source"].as<std::string>();
    folder = const_cast<char*> (string_folder.c_str());
    interval_of_capture = stoi(string_interval_of_capture);

    channel_map_rce = readout_share +  vm["rcemap"].as<std::string>();
    channel_map_felix = readout_share + vm["felixmap"].as<std::string>();
  }
  catch (bpo::error const& e) 
  {
    ers::error(dunedaq::triggertransform::IncorrectParameters(ERS_HERE, e.what()));
  }

  int num_trs = 1000;
  channelMap.reset(new swtpg::PdspChannelMapService(channel_map_rce, channel_map_felix));

  //m_host = "127.0.0.1";
  //m_port = "9092";
  //m_topic = "dunedqm-incomingadcfrequency";
  //Kafka server settings
  //std::string brokers = m_host + ":" + m_port;
  std::string errstr;

  RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
  conf->set("bootstrap.servers", broker, errstr);
  if (errstr != "")
  {
    dunedaq::triggertransform::CannotProduce(ERS_HERE, "Bootstrap server error : " + errstr);
  }
  if (const char *env_p = std::getenv("DUNEDAQ_APPLICATION_NAME"))
    conf->set("client.id", env_p, errstr);
  else
    conf->set("client.id", "rawdataProducerdefault", errstr);

  if (errstr != "")
  {
    dunedaq::triggertransform::CannotProduce(ERS_HERE, "Producer configuration error : " + errstr);
  }
  //Create producer instance
  m_producer = RdKafka::Producer::create(conf, errstr);

  if (errstr != "")
  {
    dunedaq::triggertransform::CannotProduce(ERS_HERE, "Producer creation error : " + errstr);
  }

  DIR *dir; struct dirent *diread;




  std::ofstream input_parsed_files("parsedfiles.txt", std::ios::app); 


  while (true)
  {
    std::vector<std::string> files;
    try
    {
      if ((dir = opendir(folder)) != nullptr) 
      {
        while ((diread = readdir(dir)) != nullptr) {
            files.push_back(folder + std::string(diread->d_name));
        }
        closedir (dir);
      } else 
      {
          ers::error(dunedaq::triggertransform::CannotOpenFolder(ERS_HERE, folder));
      }


      // Read from the text file


      for (std::string file : files) 
      {
        bool file_parsed = false;

        std::ifstream outut_parsed_files("parsedfiles.txt");

        std::string parsed_file_strings;

        try
        {
          if(file.find("hdf5") != std::string::npos )
          {
            while (getline(outut_parsed_files, parsed_file_strings))
            {
              // Output the text from the file
              //std::cout << "parsed_file_strings" << parsed_file_strings << "      file " << file << std::endl;

              if(parsed_file_strings == file && parsed_file_strings != "")
              {
                file_parsed = true;
                break;
              }              
            }
            if(!file_parsed)
            {
              
              //  HighFive::File file("/eos/home-y/yadonon/swtest_run000002_0000_glehmann_20211001T115720.hdf5", HighFive::File::ReadOnly);
              //  std::vector<std::string> data_path = traverseFile(file, num_trs);
              //std::cout << "Parsing file : " <<file << std::endl;
              HighFive::File h5file(file, HighFive::File::ReadOnly);
              std::vector<std::string> data_path = traverseFile(h5file, num_trs);
              input_parsed_files << file << std::endl ;
              //file_parsed = true;
            }
          }
            
        }
        catch(const std::exception& e)
        {
          dunedaq::triggertransform::ErrorReadingFile(ERS_HERE, e.what());
        } 
        outut_parsed_files.close();

      }
      sleep(5); //Wait, then search if files have been added
    }
    catch(const std::exception& e)
    {
      dunedaq::triggertransform::CannotOpenFile(ERS_HERE, e.what());
    }

  }
  input_parsed_files.close(); 

  return 0;
}
