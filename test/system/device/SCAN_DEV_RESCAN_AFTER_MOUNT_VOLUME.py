#!/usr/bin/env python3
import subprocess
import os
import sys
sys.path.append("../lib/")
sys.path.append("../volume/")

import json_parser
import ibofos
import cli
import test_result
import MOUNT_VOL_BASIC_1

def clear_result():
    if os.path.exists( __file__ + ".result"):
        os.remove( __file__ + ".result")

def set_result(detail):
    code = json_parser.get_response_code(detail)
    result = test_result.expect_true(code)
    with open(__file__ + ".result", "w") as result_file:
        result_file.write(result + " (" + str(code) + ")" + "\n" + detail)

def execute():
    clear_result()
    ibofos.start_ibofos()
    MOUNT_VOL_BASIC_1.execute()
    out = cli.scan_device()
    return out

if __name__ == "__main__":
    out = execute()
    set_result(out)
    ibofos.kill_ibofos()