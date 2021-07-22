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

// Object structure used by the test/fake producer module
local dqmprocessor = {

    running_mode : s.string("RunningMode", moo.re.ident,
                            doc="A string field"),

    conf: s.record("Conf", [
        s.field("mode", self.running_mode, "",
                doc="'debug' if in debug mode"),
    ], doc="Generic DQM configuration"),
};

moo.oschema.sort_select(dqmprocessor, ns)