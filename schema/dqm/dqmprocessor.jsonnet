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

    big_count : s.number("BigCount", "i8",
                         doc="A count of more things"),

    index : s.number("Index", "i4",
                     doc="An integer index"),

    index_list : s.sequence("IndexList", self.index,
                            doc="A list with indexes"),

    standard_dqm: s.record("StandardDQM", [
        s.field("how_often", self.time, 1,
                doc="Algorithm is run every x seconds"),
        s.field("unavailable_time", self.time, 1,
                doc="When it's time to run the algorithm but it's already running wait this time"),
        s.field("num_frames", self.count, 1,
                doc="How many frames do we process in each instance of the algorithm")
    ], doc="Standard DQM analysis"),


    conf: s.record("Conf", [
        s.field("region", self.index, 0, doc="The region index"),
        s.field("channel_map", self.string, doc='"HD" or "VD"'),
        s.field("sdqm_hist", self.standard_dqm,      # This one is for the raw event display
                doc="Standard dqm"),
        s.field("sdqm_mean_rms", self.standard_dqm,  # This one is for the Mean and RMS
                doc="Standard dqm"),
        s.field("sdqm_fourier", self.standard_dqm,   # This one is for fourier transforms
                doc="Fourier"),
        s.field("kafka_address", self.string,
                doc="Address used for sending to the kafka broker"),
        s.field("link_idx", self.index_list,
                doc="Index of each link that is sending data"),
        s.field("clock_frequency", self.big_count,
                doc="Clock frequency in Hz")
    ], doc="Generic DQM configuration")
};

moo.oschema.sort_select(dqmprocessor, ns)