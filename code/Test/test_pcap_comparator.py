#!/usr/bin/python3

import sys, os
import subprocess, shlex
import time
import unittest
import random

from Test.TestUtil import PcapComparator, ID2TExecution

# this dictionary holds the generators (functions) for the parameters
# that will be passed to the MembershipMgmtCommAttack
# items need the parameter-name as key and a function that will be called
# without parameters and returns a valid value for that parameter as value
# WARNING: parameters will be passed via command line, make sure your values
# get converted to string correctly
_random_bool = lambda: random.random() < 0.5
ID2T_PARAMETER_GENERATORS = {
    "bots.count": lambda: random.randint(1, 6),
    "hidden_mark": _random_bool,
    "interval.selection.end": lambda: random.randint(100, 1501),  # values are taken from default trace
    "interval.selection.start": lambda: random.randint(0, 1401),
    "interval.selection.strategy": lambda: random.choice(["optimal", "custom", "random"]),
    "ip.reuse.external": lambda: random.uniform(0, 1),
    "ip.reuse.local": lambda: random.uniform(0, 1),
    "ip.reuse.total": lambda: random.uniform(0, 1),
    "multiport": _random_bool,
    "nat.present": _random_bool,
    "packet.padding": lambda: random.randint(0, 100),
    "packets.limit": lambda: random.randint(50, 250),
    "ttl.from.caida": _random_bool,
}


class PcapComparison(unittest.TestCase):
    ID2T_PATH = ".."
    ID2T_LOCATION = ID2T_PATH + "/" + "id2t"

    NUM_ITERATIONS_PER_PARAMS = 3
    NUM_ITERATIONS = 4

    PCAP_ENVIRONMENT_VALUE = "ID2T_SRC_PCAP"
    SEED_ENVIRONMENT_VALUE = "ID2T_SEED"

    DEFAULT_PCAP = "resources/test/Botnet/telnet-raw.pcap"
    DEFAULT_SEED = "42"

    def __init__(self, *args, **kwargs):
        unittest.TestCase.__init__(self, *args, **kwargs)

        # params to call id2t with, as a list[list[str]]
        # do a round of testing for each list[str] we get
        # if none generate some params itself
        self.id2t_params = None

    def set_id2t_params(self, params: "list[list[str]]"):
        self.id2t_params = params

    def setUp(self):
        self.executions = []

    def test_determinism(self):
        input_pcap = os.environ.get(self.PCAP_ENVIRONMENT_VALUE, self.DEFAULT_PCAP)
        seed = os.environ.get(self.SEED_ENVIRONMENT_VALUE, self.DEFAULT_SEED)

        if self.id2t_params is None:
            self.id2t_params = self.random_id2t_params()

        for params in self.id2t_params:
            self.do_test_round(input_pcap, seed, params)

    def do_test_round(self, input_pcap, seed, additional_params):
        generated_pcap = None
        for i in range(self.NUM_ITERATIONS_PER_PARAMS):
            execution = ID2TExecution(input_pcap, seed=seed)
            self.print_warning("The command that gets executed is:", execution.get_run_command(additional_params))
            self.executions.append(execution)

            try:
                execution.run(additional_params)
            except AssertionError as e:
                self.print_warning(execution.get_output())
                self.assertEqual(execution.get_return_code(), 0, "For some reason id2t completed with an error")
                raise e

            self.print_warning(execution.get_output())

            pcap = execution.get_pcap_filename()

            if generated_pcap is not None:
                if "No packets were injected." in pcap or "No packets were injected." in generated_pcap:
                    self.assertEqual(pcap, generated_pcap)
                else:
                    try:
                        self.compare_pcaps(generated_pcap, pcap)
                    except AssertionError as e:
                        execution.keep_file(pcap)
                        self.executions[-2].keep_file(generated_pcap)
                        raise e
            else:
                generated_pcap = pcap

            self.print_warning()
            time.sleep(1)  # let some time pass between calls because files are based on the time

    def tearDown(self):
        self.print_warning("Cleaning up files generated by the test-calls...")
        for id2t_run in self.executions:
            for file in id2t_run.get_files_for_deletion():
                self.print_warning(file)

            id2t_run.cleanup()

        self.print_warning("Done")

        kept = [file for file in id2t_run.get_kept_files() for id2t_run in self.executions]
        self.print_warning("The following files have been kept: " + ", ".join(kept))

    def compare_pcaps(self, one: str, other: str):
        PcapComparator().compare_files(self.ID2T_PATH + "/" + one, self.ID2T_PATH + "/" + other)

    def print_warning(self, *text):
        print(*text, file=sys.stderr)

    def random_id2t_params(self):
        """
        :return: A list of parameter-lists for id2t, useful if you want several
        iterations
        """
        param_list = []
        for i in range(self.NUM_ITERATIONS):
            param_list.append(self.random_id2t_param_set())
        return param_list

    def random_id2t_param_set(self):
        """
        Create a list of parameters to call the membersmgmtcommattack with
        :return: a list of command-line parameters
        """
        param = lambda key, val: "%s=%s" % (str(key), str(val))

        number_of_keys = min(random.randint(2, 5), len(ID2T_PARAMETER_GENERATORS))
        keys = random.sample(list(ID2T_PARAMETER_GENERATORS), number_of_keys)

        params = []
        for key in keys:
            generator = ID2T_PARAMETER_GENERATORS[key]
            params.append(param(key, generator()))

        return params


if __name__ == "__main__":
    import sys

    # parameters for this program are interpreted as id2t-parameters
    id2t_args = sys.argv[1:]
    comparison = PcapComparison("test_determinism")
    if id2t_args: comparison.set_id2t_params([id2t_args])

    suite = unittest.TestSuite()
    suite.addTest(comparison)

    unittest.TextTestRunner().run(suite)
