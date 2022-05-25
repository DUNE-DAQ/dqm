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

    time : s.number("Time", "f4", doc="A time"),

    count : s.number("Count", "i4",
                     doc="A count of not too many things"),

    index : s.number("Index", "i4",
                     doc="An integer index"),

    big_count : s.number("BigCount", "i8",
                         doc="A count of more things"),

    index_list : s.sequence("IndexList", self.index,
                            doc="A list with indexes"),

    netmgr_name : s.string("NetMgrName", doc="Connection or topic name to be used with NetworkManager"),

    standard_dqm: s.record("StandardDQM", [
        s.field("how_often", self.time, 0,
                doc="Algorithm is run every x seconds"),
        s.field("num_frames", self.count, 0,
                doc="How many frames do we process in each instance of the algorithm")
    ], doc="Standard DQM analysis"),


    conf: s.record("Conf", [
        s.field("region", self.index, 0, doc="The region index"),
        s.field("channel_map", self.string, doc='"HD" or "VD"'),
        s.field("mode", self.string, doc='readout or df',),
        s.field("hist", self.standard_dqm,      # This one is for the raw event display
                doc="Standard dqm"),
        s.field("mean_rms", self.standard_dqm,  # This one is for the Mean and RMS
                doc="Standard dqm"),
        s.field("fourier", self.standard_dqm,   # This one is for fourier transforms
                doc="Fourier"),
        s.field("fourier_sum", self.standard_dqm, # This one is for summed fourier transforms
                doc="Fourier sum"),
        s.field("sdqm_channel_mask", self.standard_dqm, # This one is for the WIB2 channel masks
                doc="Channel mask"),
        s.field("kafka_address", self.string,
                doc="Address used for sending to the kafka broker"),
        s.field("link_idx", self.index_list,
                doc="Index of each link that is sending data"),
        s.field("clock_frequency", self.big_count,
                doc="Clock frequency in Hz"),
        s.field("timesync_topic_name", self.netmgr_name,
                doc="Topic to use for receiving TimeSync messages"),
        s.field("df2dqm_connection_name", self.netmgr_name,
                doc="Connection to use for receiving TRs from DF"),
        s.field("dqm2df_connection_name", self.netmgr_name,
                doc="Connection to use for sending TRMon messages to DF"),
        s.field("readout_window_offset", self.big_count,
                doc="Offset to use for the windows requested to readout"),

        s.field("df_seconds", self.time,
                doc="Number of seconds between requests to DF for TRs"),
        s.field("df_offset", self.time,
                doc="Number of seconds to offset so that when there are multiple DF apps the rate is maintained"),
        s.field("df_algs", self.big_count,
                doc="Bitfield where the bits are whether an algorith is turned on or off for TRs coming from DF"),
        s.field("df_num_frames", self.count,
                doc="Bitfield where the bits are whether an algorith is turned on or off for TRs coming from DF"),
        s.field("frontend_type", self.string,
                doc="Frontend to be used for DQM, takes the same values as in readout")


    ], doc="Generic DQM configuration")
};

moo.oschema.sort_select(dqmprocessor, ns)