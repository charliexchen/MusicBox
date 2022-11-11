import sys
import threading
import time
import os
class AudacityPipeline:
    def __init__(self):
        self._get_pipes()
        self._to_file = open(self._to_file, "w")
        print("-- File to write to has been opened")
        self._from_file = open(self._from_file, "rt")
        print("-- File to read from has now been opened too\r\n")

    def _send_command(self, command):
        """Send a single command."""
        print("Send: >>> \n" + command)
        self._to_file.write(command + self._eol)
        self._to_file.flush()

    def _get_response(self):
        """Return the command response."""
        result = ""
        line = ""
        while True:
            result += line
            line = self._from_file.readline()
            if line == "\n" and len(result) > 0:
                break
        return result

    def do_command(self, command):
        """Send one command, and return the response."""
        self._send_command(command)
        response = self._get_response()
        print("Rcvd: <<< \n" + response)
        return response

    def _get_pipes(self):
        if sys.platform == "win32":
            print("pipe-test.py, running on windows")
            self._to_file = "\\\\.\\pipe\\ToSrvPipe"
            self._from_file = "\\\\.\\pipe\\FromSrvPipe"
            self._eol = "\r\n\0"
        else:
            print("pipe-test.py, running on linux or mac")
            self._to_file = "/tmp/audacity_script_pipe.to." + str(os.getuid())
            self._from_file = "/tmp/audacity_script_pipe.from." + str(os.getuid())
            self._eol = "\n"
aud = AudacityPipeline()
aud.do_command("New")
aud.do_command("Record2ndChoice")
time.sleep(3)
aud.do_command("Stop")
aud.do_command("Record2ndChoice")
time.sleep(3)
aud.do_command("Stop")
time.sleep(0.1)
aud.do_command(f"Select: Track=0 mode=Set")
aud.do_command("SelTrackStartToEnd")
time.sleep(1)
aud.do_command(f"ChangePitch:Percentage=200")
time.sleep(0.1)
aud.do_command(f"Select: Track=1 mode=Set")
time.sleep(1)
aud.do_command(f"ChangePitch:Percentage=-50")
