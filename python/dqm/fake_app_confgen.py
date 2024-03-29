# Set moo schema search path
from dunedaq.env import get_moo_model_path
import moo.io
moo.io.default_load_path = get_moo_model_path()

# Load configuration types
import moo.otypes

moo.otypes.load_types('rcif/cmd.jsonnet')
moo.otypes.load_types('appfwk/cmd.jsonnet')
moo.otypes.load_types('appfwk/app.jsonnet')

moo.otypes.load_types('dfmodules/triggerrecordbuilder.jsonnet')
moo.otypes.load_types('readoutlibs/readoutconfig.jsonnet')
moo.otypes.load_types('readoutlibs/sourceemulatorconfig.jsonnet')

moo.otypes.load_types('dqm/dqmprocessor.jsonnet')

# Import new types
import dunedaq.cmdlib.cmd as basecmd # AddressedCmd, 
import dunedaq.rcif.cmd as rccmd # AddressedCmd, 
import dunedaq.appfwk.cmd as cmd # AddressedCmd, 
import dunedaq.appfwk.app as app # AddressedCmd, 
import dunedaq.dfmodules.triggerrecordbuilder as trb
import dunedaq.readoutlibs.readoutconfig as rconf
import dunedaq.readoutlibs.sourceemulatorconfig as sec
import dunedaq.dqm.dqmprocessor as dqmprocessor

from appfwk.utils import mcmd, mrccmd, mspec

import json
import math
# Time to wait on pop()
QUEUE_POP_WAIT_MS=100;
# local clock speed Hz
CLOCK_SPEED_HZ = 62500000;

def generate(
        FRONTEND_TYPE='wib',
        NUMBER_OF_DATA_PRODUCERS=1,
        NUMBER_OF_TP_PRODUCERS=0,
        DATA_RATE_SLOWDOWN_FACTOR = 1,
        ENABLE_SOFTWARE_TPG=False,
        RUN_NUMBER = 333,
        DATA_FILE="./frames.bin"
    ):
    
    # Define modules and queues
    queue_bare_specs = [
            app.QueueSpec(inst="time_sync_dqm_q", kind='FollyMPMCQueue', capacity=100),
            app.QueueSpec(inst="trigger_decision_q_dqm", kind='FollySPSCQueue', capacity=20),
            app.QueueSpec(inst="trigger_record_q_dqm", kind='FollySPSCQueue', capacity=20),
            app.QueueSpec(inst="data_fragments_q", kind='FollyMPMCQueue', capacity=20*NUMBER_OF_DATA_PRODUCERS),
            app.QueueSpec(inst="data_fragments_q_dqm", kind='FollyMPMCQueue', capacity=20*NUMBER_OF_DATA_PRODUCERS),
        ] + [
            app.QueueSpec(inst=f"wib_fake_link_{idx}", kind='FollySPSCQueue', capacity=100000)
                for idx in range(NUMBER_OF_DATA_PRODUCERS)
        ] + [

            app.QueueSpec(inst=f"data_requests_{idx}", kind='FollySPSCQueue', capacity=1000)
                for idx in range(NUMBER_OF_DATA_PRODUCERS)
        ] + [
            app.QueueSpec(inst=f"{FRONTEND_TYPE}_link_{idx}", kind='FollySPSCQueue', capacity=100000)
                for idx in range(NUMBER_OF_DATA_PRODUCERS)
        ] + [
            app.QueueSpec(inst=f"{FRONTEND_TYPE}_recording_link_{idx}", kind='FollySPSCQueue', capacity=100000)
                for idx in range(NUMBER_OF_DATA_PRODUCERS)
        ]
    

    # Only needed to reproduce the same order as when using jsonnet
    queue_specs = app.QueueSpecs(sorted(queue_bare_specs, key=lambda x: x.inst))


    mod_specs = [
        mspec("fake_source", "FakeCardReader", [
                        app.QueueInfo(name=f"output_{idx}", inst=f"{FRONTEND_TYPE}_link_{idx}", dir="output")
                            for idx in range(NUMBER_OF_DATA_PRODUCERS)
                        ]),
        ] + [
        mspec(f"datahandler_{idx}", "DataLinkHandler", [
                        app.QueueInfo(name="raw_input", inst=f"{FRONTEND_TYPE}_link_{idx}", dir="input"),
                        app.QueueInfo(name="timesync", inst="time_sync_dqm_q", dir="output"),
                        # app.QueueInfo(name="requests", inst=f"data_requests_{idx}", dir="input"),
                        # app.QueueInfo(name="fragments_dqm", inst="data_fragments_q_dqm", dir="output"),
                        app.QueueInfo(name="data_requests_0", inst=f"data_requests_{idx}", dir="input"),
                        app.QueueInfo(name="data_response_0", inst="data_fragments_q_dqm", dir="output"),
                        app.QueueInfo(name="raw_recording", inst=f"{FRONTEND_TYPE}_recording_link_{idx}", dir="output")
                        ]) for idx in range(NUMBER_OF_DATA_PRODUCERS)
        ] + [
        mspec("trb_dqm", "TriggerRecordBuilder", [
                        app.QueueInfo(name="trigger_decision_input_queue", inst="trigger_decision_q_dqm", dir="input"),
                        app.QueueInfo(name="trigger_record_output_queue", inst="trigger_record_q_dqm", dir="output"),
                        app.QueueInfo(name="data_fragment_input_queue", inst="data_fragments_q_dqm", dir="input")
                    ] + [
                        app.QueueInfo(name=f"data_request_{idx}_output_queue", inst=f"data_requests_{idx}", dir="output")
                            for idx in range(NUMBER_OF_DATA_PRODUCERS)
                    ]),
        ] + [
        mspec("dqmprocessor", "DQMProcessor", [
                        app.QueueInfo(name="trigger_record_dqm_processor", inst="trigger_record_q_dqm", dir="input"),
                        app.QueueInfo(name="trigger_decision_dqm_processor", inst="trigger_decision_q_dqm", dir="output"),
                        app.QueueInfo(name="timesync_dqm_processor", inst="time_sync_dqm_q", dir="input"),
                    ]),

        ]

    init_specs = app.Init(queues=queue_specs, modules=mod_specs)

    jstr = json.dumps(init_specs.pod(), indent=4, sort_keys=True)
    print(jstr)

    initcmd = rccmd.RCCommand(
        id=basecmd.CmdId("init"),
        entry_state="NONE",
        exit_state="INITIAL",
        data=init_specs
    )

    confcmd = mrccmd("conf", "INITIAL", "CONFIGURED",[
                ("fake_source",sec.Conf(
                            link_confs=[sec.LinkConfiguration(
                                geoid=sec.GeoID(system="TPC", region=0, element=idx),
                                slowdown=DATA_RATE_SLOWDOWN_FACTOR,
                                queue_name=f"output_{idx}"
                            ) for idx in range(NUMBER_OF_DATA_PRODUCERS)],
                            # input_limit=10485100, # default
                            queue_timeout_ms=QUEUE_POP_WAIT_MS,
			                set_t0_to=0
                        )),
            ] + [
                (f"datahandler_{idx}", rconf.Conf(
                        readoutmodelconf= rconf.ReadoutModelConf(
                            source_queue_timeout_ms= QUEUE_POP_WAIT_MS,
                            fake_trigger_flag=0,
                            region_id = 0,
                            element_id = idx,
                        ),
                        latencybufferconf= rconf.LatencyBufferConf(
                            latency_buffer_size = 3*CLOCK_SPEED_HZ/(25*12*DATA_RATE_SLOWDOWN_FACTOR),
                            region_id = 0,
                            element_id = idx,
                        ),
                        rawdataprocessorconf= rconf.RawDataProcessorConf(
                            region_id = 0,
                            element_id = idx,
                            enable_software_tpg = ENABLE_SOFTWARE_TPG,
                        ),
                        requesthandlerconf= rconf.RequestHandlerConf(
                            latency_buffer_size = 3*CLOCK_SPEED_HZ/(25*12*DATA_RATE_SLOWDOWN_FACTOR),
                            pop_limit_pct = 0.8,
                            pop_size_pct = 0.1,
                            region_id = 0,
                            element_id = idx,
                            output_file = f"output_{idx}.out",
                            stream_buffer_size = 8388608,
                            enable_raw_recording = True
                        )
                        )) for idx in range(NUMBER_OF_DATA_PRODUCERS)
            ] + [
                ("trb_dqm", trb.ConfParams(
                        general_queue_timeout=QUEUE_POP_WAIT_MS,
                        map=trb.mapgeoidqueue([
                                trb.geoidinst(region=0, element=idx, system="TPC", queueinstance=f"data_requests_{idx}") for idx in range(NUMBER_OF_DATA_PRODUCERS)
                            ]),
                        )),
            ] + [
                ('dqmprocessor', dqmprocessor.Conf(
                        region=0,
                        channel_map='HD', # 'HD' for horizontal drift or 'VD' for vertical drift
                        sdqm_hist=dqmprocessor.StandardDQM(how_often=60, unavailable_time=10, num_frames=50),
                        sdqm_mean_rms=dqmprocessor.StandardDQM(how_often=10, unavailable_time=1, num_frames=100),
                        sdqm_fourier=dqmprocessor.StandardDQM(how_often=60 * 20, unavailable_time=60, num_frames=100),
                        sdqm_fourier_sum=dqmprocessor.StandardDQM(how_often=60 * 5, unavailable_time=1,num_frames=8192),
                        kafka_address='dqmbroadcast:9092',
                        link_idx=list(range(NUMBER_OF_DATA_PRODUCERS)),
                        clock_frequency=CLOCK_SPEED_HZ
                        ))
            ]
                     )
    
    jstr = json.dumps(confcmd.pod(), indent=4, sort_keys=True)
    print(jstr)

    startpars = rccmd.StartParams(run=RUN_NUMBER)
    startcmd = mrccmd("start", "CONFIGURED", "RUNNING", [
            ("fake_source", startpars),
            ("datahandler_.*", startpars),
            ("trb_dqm", startpars),
            ("dqmprocessor", startpars),
        ])

    jstr = json.dumps(startcmd.pod(), indent=4, sort_keys=True)
    print("="*80+"\nStart\n\n", jstr)


    stopcmd = mrccmd("stop", "RUNNING", "CONFIGURED", [
            ("fake_source", None),
            ("datahandler_.*", None),
            ("trb_dqm", None),
            ("dqmprocessor", None),
        ])

    jstr = json.dumps(stopcmd.pod(), indent=4, sort_keys=True)
    print("="*80+"\nStop\n\n", jstr)

    pausecmd = mrccmd("pause", "RUNNING", "RUNNING", [
            ("", None)
        ])

    jstr = json.dumps(pausecmd.pod(), indent=4, sort_keys=True)
    print("="*80+"\nPause\n\n", jstr)

    resumecmd = mrccmd("resume", "RUNNING", "RUNNING", [
        ])

    jstr = json.dumps(resumecmd.pod(), indent=4, sort_keys=True)
    print("="*80+"\nResume\n\n", jstr)

    scrapcmd = mrccmd("scrap", "CONFIGURED", "INITIAL", [
            ("", None)
        ])

    jstr = json.dumps(scrapcmd.pod(), indent=4, sort_keys=True)
    print("="*80+"\nScrap\n\n", jstr)

    # Create a list of commands
    cmd_seq = [initcmd, confcmd, startcmd, stopcmd, pausecmd, resumecmd, scrapcmd]

    # Print them as json (to be improved/moved out)
    jstr = json.dumps([c.pod() for c in cmd_seq], indent=4, sort_keys=True)
    return jstr
        
if __name__ == '__main__':
    # Add -h as default help option
    CONTEXT_SETTINGS = dict(help_option_names=['-h', '--help'])

    import click

    @click.command(context_settings=CONTEXT_SETTINGS)
    @click.option('-f', '--frontend-type', type=click.Choice(['wib', 'wib2', 'pds_queue', 'pds_list'], case_sensitive=True), default='wib')
    @click.option('-n', '--number-of-data-producers', default=1)
    @click.option('-t', '--number-of-tp-producers', default=0)
    @click.option('-s', '--data-rate-slowdown-factor', default=10)
    @click.option('-g', '--enable-software-tpg', is_flag=True)
    @click.option('-r', '--run-number', default=333)
    @click.option('-d', '--data-file', type=click.Path(), default='./frames.bin')
    @click.argument('json_file', type=click.Path(), default='dqm.json')
    def cli(frontend_type, number_of_data_producers, number_of_tp_producers,
            data_rate_slowdown_factor, enable_software_tpg, run_number, data_file,
            json_file):
        """
          JSON_FILE: Input raw data file.
          JSON_FILE: Output json configuration file.
        """

        with open(json_file, 'w') as f:
            f.write(generate(
                    FRONTEND_TYPE = frontend_type,
                    NUMBER_OF_DATA_PRODUCERS = number_of_data_producers,
                    NUMBER_OF_TP_PRODUCERS = number_of_tp_producers,
                    DATA_RATE_SLOWDOWN_FACTOR = data_rate_slowdown_factor,
                    ENABLE_SOFTWARE_TPG = enable_software_tpg,
                    RUN_NUMBER = run_number,
                    DATA_FILE = data_file,
                ))

        print(f"'{json_file}' generation completed.")

    cli()
