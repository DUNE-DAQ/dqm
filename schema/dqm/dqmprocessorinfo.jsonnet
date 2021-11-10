// This is the application info schema used by the ta link handler module.
// It describes the information object structure passed by the application 
// for operational monitoring

local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.dqm.dqmprocessorinfo");

local info = {
    uint8  : s.number("uint8", "u8",
                     doc="An unsigned of 8 bytes"),

   info: s.record("Info", [
       s.field("requests",              self.uint8, 0, doc="Number of requests"), 
       s.field("total_requests",        self.uint8, 0, doc="Total number of requests"), 
       s.field("data_deliveries",       self.uint8, 0, doc="Number of requests"), 
       s.field("total_data_deliveries", self.uint8, 0, doc="Number of requests"), 
   ], doc="DQM information")
};

moo.oschema.sort_select(info) 
