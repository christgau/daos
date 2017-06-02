#!/usr/bin/env python3
# Copyright (C) 2016-2017 Intel Corporation
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted for any purpose (including commercial purposes)
# provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions, and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions, and the following disclaimer in the
#    documentation and/or materials provided with the distribution.
#
# 3. In addition, redistributions of modified forms of the source or binary
#    code must carry prominent notices stating that the original code was
#    changed and the date of the change.
#
#  4. All publications or advertising materials mentioning features or use of
#     this software are asked, but not required, to acknowledge that it was
#     developed by Intel Corporation and credit the contributors.
#
# 5. Neither the name of Intel Corporation, nor the name of any Contributor
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# -*- coding: utf-8 -*-
"""
class to execute a command using orte
"""

import os
import subprocess
import shlex
import time
import getpass

#pylint: disable=broad-except

class OrteRunner():
    """setup for using ompi from test runner"""
    test_info = None
    logger = None
    log_dir_orte = ""
    testsuite = ""

    @staticmethod
    def add_cmd(cmd_list, cmd, parameters="", nextCmd=False):
        """add the ommand and parameters to the list
           Note: entries need to start with a space"""
        if nextCmd:
            cmd_list.append(" :")
        cmd_list.append(" {!s}".format(cmd))
        if parameters:
            cmd_list.append(" {!s}".format(parameters))
        return cmd_list

    @staticmethod
    def add_env_vars(cmd_list, env_vars):
        """add the environment variables to the command list
           Note: entries need to start with a space"""
        for (key, value) in env_vars.items():
            if value:
                cmd_list.append(" -x {!s}={!s}".format(key, value))
            else:
                cmd_list.append(" -x {!s}".format(key))
        return cmd_list

    def add_nodes(self, cmd_list, nodes, procs=1):
        """add the node prefix to the command list
           Note: entries need to start with a space"""
        if nodes[0].isupper():
            node_list = self.test_info.get_defaultENV(nodes)
        else:
            node_list = nodes

        cmd_list.append(" -H {!s} -N {!s}".format(node_list, procs))
        return cmd_list

    def start_cmd_list(self, log_path, testsuite, prefix):
        """add the log directory to the prefix
           Note: entries, after the first, need to start with a space"""
        self.testsuite = testsuite
        self.log_dir_orte = os.path.abspath(log_path)
        os.makedirs(self.log_dir_orte, exist_ok=True)

        cmd_list = []
        cmd_list.append("{!s}orterun".format(prefix))
        if self.test_info.get_defaultENV('TR_USE_URI', ""):
            cmd_list.append(" --hnp file:{!s}".format(
                self.test_info.get_defaultENV('TR_USE_URI')))
        cmd_list.append(" --output-filename {!s}".format(self.log_dir_orte))
        if getpass.getuser() == "root":
            cmd_list.append(" --allow-run-as-root")
        return cmd_list

    def start_process(self, cmd_list):
        """Launch process set """
        cmdstr = ''.join(cmd_list)
        self.logger.info("OrteRunner: start: %s", cmdstr)
        cmdarg = shlex.split(cmdstr)
        fileout = os.path.join(self.log_dir_orte,
                               "{!s}.out".format(self.testsuite))
        fileerr = os.path.join(self.log_dir_orte,
                               "{!s}.err".format(self.testsuite))
        with open(fileout, mode='a') as outfile, \
            open(fileerr, mode='a') as errfile:
            outfile.write("{!s}\n  Command: {!s} \n{!s}\n".format(
                ("=" * 40), cmdstr, ("=" * 40)))
            outfile.flush()
            errfile.write("{!s}\n  Command: {!s} \n{!s}\n".format(
                ("=" * 40), cmdstr, ("=" * 40)))
            proc = subprocess.Popen(cmdarg, stdout=outfile, stderr=errfile)
        return proc

    def check_process(self, proc):
        """Check if a process is still running"""
        proc.poll()
        procrtn = proc.returncode
        if procrtn is None:
            return True
        self.logger.info("Process has exited")
        return False

    def wait_process(self, proc, waittime=180):
        """wait for processes to terminate
        Wait for the process to exit, and return the return code.
        """
        self.logger.info("Test: waiting for process :%s", proc.pid)

        try:
            procrtn = proc.wait(waittime)
        except subprocess.TimeoutExpired as e:
            self.logger.info("Test: process timeout: %s\n", e)
            procrtn = self.stop_process("process timeout", proc)

        self.logger.info("Test: return code: %s\n", procrtn)
        return procrtn


    def stop_process(self, msg, proc):
        """ wait for process to terminate """
        self.logger.info("%s: %s - stopping processes :%s", \
          self.testsuite, msg, proc.pid)
        i = 60
        procrtn = None
        while i:
            proc.poll()
            procrtn = proc.returncode
            if procrtn is not None:
                break
            else:
                time.sleep(1)
                i = i - 1

        if procrtn is None:
            self.logger.info("%s: Again stopping processes :%s", \
              self.testsuite, proc.pid)
            procrtn = -1
            try:
                proc.terminate()
                proc.wait(2)
            except ProcessLookupError:
                pass
            except Exception:
                self.logger.info("%s: killing processes :%s", \
                  self.testsuite, proc.pid)
                proc.kill()

        self.logger.info("%s: %s - return code: %d\n", \
          self.testsuite, msg, procrtn)
        return procrtn
