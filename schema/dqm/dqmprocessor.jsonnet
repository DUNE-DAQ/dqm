// The schema used by classes in the appfwk code tests.
//
// It is an example of the lowest layer schema below that of the "cmd"
// and "app" and which defines the final command object structure as
// consumed by instances of specific DAQModule implementations (ie,
// the test/Fake* modules).

local moo = import "moo.jsonnet";

// A schema builder in the given path (namespace)
local ns = "dunedaq.dqm.dqmprocessor";
local s = moo.oschema.schema(ns);

// Object structure used by the dqm processor plugin
local dqmprocessor = {

    string : s.string("string", doc="A string"),

    time : s.number("Time", "i4", doc="A time"),

    count : s.number("Count", "i4",
                     doc="A count of not too many things"),

    index : s.number("Index", "i4",
                     doc="An integer index"),

    big_count : s.number("BigCount", "i8",
                         doc="A count of more things"),

    index_list : s.sequence("IndexList", self.index,
                            doc="A list with indexes"),

    netmgr_name : s.string("NetMgrName", doc="Connection or topic name to be used with NetworkManager"),

    sourceid_number : s.number("sourceid_number", "u4",
                     doc="Source identifier"),

    standard_dqm: s.record("StandardDQM", [
        s.field("how_often", self.time, 0,
                doc="Algorithm is run every x seconds"),
        s.field("num_frames", self.count, 0,
                doc="How many frames do we process in each instance of the algorithm")
    ], doc="Standard DQM analysis"),


    conf: s.record("Conf", [
        s.field("channel_map", self.string, doc='"HD" or "VD"'),
        s.field("mode", self.string, doc='readout or df',),
        s.field("raw", self.standard_dqm, doc="Parameters for sending raw data"),
        s.field("rms", self.standard_dqm, doc="Parameters for sending the RMS of the ADC distribution"),
        s.field("std", self.standard_dqm, doc="Parameters for sending the STD of the ADC distribution"),
        s.field("fourier_channel", self.standard_dqm, doc="Parameters for sending the fourier transform for each channel"),
        s.field("fourier_plane", self.standard_dqm, doc="Parameters for sending the fourier transform for each plane"),
        s.field("kafka_address", self.string, doc="Address used for sending messages to the kafka broker"),
        s.field("kafka_topic", self.string, doc="Topic used for sending messages to the kafka broker"),
        s.field("link_idx", self.index_list, doc="Index of each link that is sending data"),
        s.field("clock_frequency", self.big_count, doc="Clock frequency in Hz"),
        s.field("df2dqm_connection_name", self.netmgr_name, doc="Connection to use for receiving TRs from DF"),
        s.field("dqm2df_connection_name", self.netmgr_name, doc="Connection to use for sending TRMon messages to DF"),
        s.field("readout_window_offset", self.big_count, doc="Offset to use for the windows requested to readout"),
        s.field("df_seconds", self.time, doc="Number of seconds between requests to DF for TRs"),
        s.field("df_offset", self.time, doc="Number of seconds to offset so that when there are multiple DF apps the rate is maintained"),
        s.field("df_algs", self.string, doc="Bitfield where the bits are whether an algorith is turned on or off for TRs coming from DF"),
        s.field("df_num_frames", self.count, doc="Number of frames for the fragments coming from DF"),
        s.field("max_num_frames", self.count, 0, doc="Maximum number of frames used in the algorithms for the fragments"),
        s.field("frontend_type", self.string, doc="Frontend to be used for DQM, takes the same values as in readout"),
        s.field("trb_sourceid", self.sourceid_number, 0, doc="The SourceID of the TRB associated with an instance of the DQMProcessor")
    ], doc="Generic DQM configuration")
};

moo.oschema.sort_select(dqmprocessor, ns)