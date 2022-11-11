"""Rewriting MIDIParser."""

import py_midicsv as pm
import sys
import threading
import time
import PyMidiBox
import yaml
import music_configs
from typing import Union, Tuple, Sequence, Dict, Any, Optional

from typing import TypedDict

Note = int
InstrumentID = int


class MIDINote(TypedDict):
  instrument_id: InstrumentID
  note: Note
  timing_ms: int


Commands = Sequence[Tuple[float, int, int]]

Schedule = Sequence[MIDINote]
InstrumentLookUp = Dict[InstrumentID, str]

MIDDLE_C = 60
MIN_DELAY_MS = 240  # If a note is replayed within this time, then send the reset command immediately.
MAX_DELAY_MS = 100000  # Retract the note after this time

CAPITAL_A = 65
LOWER_CASE_A = 97


class MIDIMusicBoxController(PyMidiBox.MusicBoxController):
  def __init__(self, config_path: str, midi_path: str, record: bool = True):
    with open(config_path) as file:
      config = yaml.load(file, Loader=yaml.FullLoader)
    music_note_config = config["MUSIC_NOTE_CONFIG"]
    port = config["SERIAL_PORT"]
    baud_rate = config["BAUD_RATE"]
    super().__init__(music_note_config, port, baud_rate)
    self.schedule, self.instrument_look_up = self.parse_midi(midi_path)
    self.print_instruments()
    self.record = record
    if self.record:
      try:
        self.audacity = AudacityPipeline()
      except FileNotFoundError:
        print("Error: Audacity not started -- playing without recording.")
        self.record = False

    print("waiting a few seconds for the music box to initialize...")
    time.sleep(3)
    print("Done!")

  def print_instruments(self):
    print("Parsed Midi with following instruments:")
    for instrument_id in sorted(self.instrument_look_up.keys()):
      print(self.instrument_look_up[instrument_id], instrument_id)

  def get_mismatched_notes_count(
      self,
      schedule: Schedule,
      transpose: int = 0,
  ) -> Tuple[int, int]:
    """Figures out how many mismatches notes there in the MIDI file. Returns tuple of the missed notes and missed notes
    in Teble section (which tends to have more effect on the melody）。"""
    mismatches = 0
    high_mismatch = 0
    for midi_note in schedule:
      transposed_note = midi_note["note"] + transpose
      if transposed_note not in self.music_note_config:
        if transposed_note > MIDDLE_C:
          high_mismatch += 1
        mismatches += 1
    return mismatches, high_mismatch

  def find_optimal_transpose(
      self,
      schedule: Schedule,
      scan_range: Sequence[int] = range(50, -50, -1),
      log_all: bool = False,
  ) -> Tuple[int, int]:
    """Scans through transposes, and find the transposition which minimises the number of notes we have to remap."""
    min_transpose = []
    min_miss_count = float("inf")
    min_transpose_high_only = []
    min_miss_count_high_only = float("inf")
    for transpose in scan_range:
      mismatches, high_mismatch = self.get_mismatched_notes_count(
        schedule, transpose
      )
      if mismatches < min_miss_count:
        min_miss_count = mismatches
        min_transpose = [transpose]
      elif mismatches == min_miss_count:
        min_transpose.append(transpose)
      if high_mismatch < min_miss_count_high_only:
        min_miss_count_high_only = high_mismatch
        min_transpose_high_only = [transpose]
      elif high_mismatch == min_miss_count_high_only:
        min_transpose_high_only.append(transpose)

      if log_all:
        print(
          f"Transposing {transpose} results in {mismatches} misses and {high_mismatch} treble misses."
        )
    min_transpose = min(min_transpose, key=lambda x: abs(x))
    min_transpose_high_only = min(min_transpose_high_only, key=lambda x: abs(x))
    print(
      f"Default transpose: {min_transpose} Semitones with {min_miss_count} misses."
    )
    print(
      f"Default treble transpose: {min_transpose_high_only} Semitones with {min_miss_count_high_only} misses."
    )
    return min_transpose, min_transpose_high_only

  def parse_row(
      self,
      row: str,
      instrument_look_up: dict[InstrumentID, str],
  ) -> Tuple[Union[MIDINote, bool], InstrumentLookUp]:
    """Parses a single row of MIDI into note, timestamp and power."""
    cleaned_row = str.strip(row)
    arr = cleaned_row.split(", ")
    if arr[2] == "Note_on_c":
      midi_note = MIDINote(
        note=int(arr[4]),
        instrument_id=int(arr[3]),
        timing_ms=int(arr[1]),
      )
      power = int(arr[5])
      if power > 0:
        # We only care about the note being pressed down -- the music box sounds nicer that way.
        return midi_note, instrument_look_up
    elif arr[2] == "Instrument_name_t" or arr[2] == "Title_t":
      instrument_id = int(arr[0])
      instrument_name = arr[3]
      if instrument_id in instrument_look_up:
        pass
        """raise ValueError("Instrument has appeared twice!")"""
      instrument_look_up[instrument_id] = instrument_name
    return False, instrument_look_up

  def parse_midi(
      self,
      file_path: str,
      save_csv: bool = True,
  ) -> Tuple[Schedule, InstrumentLookUp]:
    # Load the MIDI file and parse it into CSV format
    csv_string = pm.midi_to_csv(file_path)
    if save_csv:
      # Save the CSV for inspection purposes
      with open("inspection.csv", "w") as f:
        f.writelines(csv_string)

    # separate out the different instruments, so we can remove, say drums from the midi easily.
    # create a dict of instruments schedules, where each
    instrument_look_up = {}
    schedule = []
    for row in csv_string:
      parsed_row, instrument_look_up = self.parse_row(row, instrument_look_up)
      if parsed_row:
        schedule.append(parsed_row)
    filtered_instrument_lookup = {}
    for midi_note in schedule:
      if midi_note["instrument_id"] in instrument_look_up:
        filtered_instrument_lookup[
          midi_note["instrument_id"]
        ] = instrument_look_up[midi_note["instrument_id"]]
      else:
        filtered_instrument_lookup[midi_note["instrument_id"]] = "Unknown"
    return schedule, filtered_instrument_lookup

  def filter_instruments(
      self, schedule: Schedule, allow_list: Sequence[InstrumentID]
  ) -> Schedule:
    filtered_schedule = []
    for midi_note in schedule:
      if midi_note["instrument_id"] in allow_list:
        filtered_schedule.append(midi_note)
    return filtered_schedule

  def get_within_range_schedule(
      self,
      schedule: Schedule,
      transpose_override: int = 0,
  ) -> Tuple[Schedule, Schedule, Schedule]:
    in_range_schedule, low_residue, high_residue = [], [], []
    for midi_note in schedule:
      transposed_note = midi_note["note"] + transpose_override
      if transposed_note in self.music_note_config:
        in_range_schedule.append(midi_note)
      else:
        if transposed_note > MIDDLE_C:
          high_residue.append(midi_note)
        else:
          low_residue.append(midi_note)
    return in_range_schedule, low_residue, high_residue

  def schedule_to_command_list(
      self,
      schedule: Schedule,
      transpose: int = 0,
      total_passes: int = 1,
      pass_id: int = 0,
      tempo_multiplier=1.0,
  ) -> Commands:
    sorted_schedule = sorted(schedule, key=lambda x: x["timing_ms"])
    per_note_schedule = {}
    for midi_note in sorted_schedule:
      remapped_note = self.remap_to_music_box_range(transpose + midi_note["note"])
      if remapped_note not in per_note_schedule:
        per_note_schedule[remapped_note] = [midi_note["timing_ms"]]
      else:
        if midi_note["timing_ms"] != per_note_schedule[remapped_note][-1]:
          per_note_schedule[remapped_note].append(midi_note["timing_ms"])
    commands = []
    for note in per_note_schedule:
      times = [
        time
        for i, time in enumerate(sorted(per_note_schedule[note]))
        if i % total_passes == pass_id
      ]
      if len(times) == 0:
        continue
      previous_note_time = times[0]
      commands.append((times[0], note, 1))
      for note_time in times[1:]:

        if note_time - previous_note_time < MIN_DELAY_MS * tempo_multiplier:
          reset_time = previous_note_time
        elif note_time - previous_note_time > MAX_DELAY_MS * tempo_multiplier:
          reset_time = previous_note_time + (MAX_DELAY_MS * tempo_multiplier)
        else:
          reset_time = note_time - (MIN_DELAY_MS * tempo_multiplier)

        commands.append((reset_time, note, 0))
        commands.append((note_time, note, 1))
        previous_note_time = note_time
    return sorted(commands, key=lambda x: x[0])

  def play_commands(self, commands: Commands, tempo_multiplier: float = 1.0) -> None:
    start_time = time.time()
    for command in commands:

      scheduled_time = start_time + (command[0] / (tempo_multiplier * 1000)) + 1
      wait_time = scheduled_time - time.time()
      if wait_time > 0:
        time.sleep(wait_time)
      self.send_note_alpha(command[1], command[2])

  def play_commands_chorded(
      self,
      commands: Commands,
      tempo_multiplier: float = 1.0,
      cap_time: Optional[int] = None

  ) -> None:
    if len(commands) < 1:
      return

    def to_alpha_command(note, power):
      clamped_note = self.remap_to_music_box_range(note)
      base_command = self.music_note_config[clamped_note]["id"]
      if power == 0:
        return base_command + CAPITAL_A
      else:
        return base_command + LOWER_CASE_A

    chorded_commands = [
      [commands[0][0], [to_alpha_command(commands[0][1], commands[0][2])]]
    ]
    for command in commands[1:]:
      if command[0] == chorded_commands[-1][0]:
        chorded_commands[-1][1].append(to_alpha_command(command[1], command[2]))
      else:
        chorded_commands.append(
          [command[0], [to_alpha_command(command[1], command[2])]]
        )
    compiled_commands = []
    for command in chorded_commands:
      compiled_commands.append((command[0], bytes(command[1])))
    if self.record:
      self.audacity.do_command("Record2ndChoice")
    time.sleep(1)
    start_time = time.time()

    for command_id, command in enumerate(compiled_commands):
      scheduled_time = start_time + (command[0] / (tempo_multiplier * 1000)) + 1
      wait_time = scheduled_time - time.time()
      if wait_time > 0:
        time.sleep(wait_time)
      self.serial_conn.write(command[1])
    time.sleep(5)
    if self.record:
      self.audacity.do_command("Stop")

  def play_with_transpose(
      self,
      tempo_multiplier: int = 1,
      transpose: Optional[int] = None,
      default_high_transpose: bool = False,
      total_passes: int = 2,
      instrument_allow_list: Optional[Sequence[InstrumentID]] = None,
  ):

    if instrument_allow_list is not None:
      filtered_schedule = self.filter_instruments(
        self.schedule, instrument_allow_list
      )
    else:
      filtered_schedule = self.schedule
    self.maybe_initialize_record()
    if transpose is None:
      transpose, high_transpose = self.find_optimal_transpose(
        filtered_schedule
      )
      if default_high_transpose:
        transpose = high_transpose

    for pass_id in range(total_passes):
      print(f"playing pass {pass_id + 1} of {total_passes}")
      commands = self.schedule_to_command_list(
        filtered_schedule,
        transpose=transpose,
        total_passes=total_passes,
        pass_id=pass_id,
        tempo_multiplier=tempo_multiplier,
      )
      self.play_commands_chorded(
        commands,
        tempo_multiplier,
      )
      self.reset_music_box()
    self.maybe_apply_audio_processing()

  def maybe_apply_audio_processing(
      self,
      high_pass_threshold: int = 50,
      low_pass_threshold: int = 10000,
  ):
    if self.record:
      pass
      """
      self.audacity.do_command("SelectAll")
      self.audacity.do_command(
          f"High-passFilter: frequency = {high_pass_threshold}, rolloff=dB12"
      )
      self.audacity.do_command("SelectAll")
      self.audacity.do_command(
          f"Low-passFilter: frequency = {low_pass_threshold}, rolloff=dB12"
      )
      self.audacity.do_command("SelectAll")
      self.audacity.do_command("Reverb: RoomSize=5")
      """

  def maybe_initialize_record(self):
    if self.record:
      print("Creating new audacity project...")
      self.audacity.do_command("New")

  def play_in_blocks(
      self,
      tempo_multiplier: int = 1,
      total_passes: Tuple[int, int, int] = (1, 1, 1),
      transpose_override: Optional[int] = None,
      instrument_allow_list: Optional[Sequence[InstrumentID]] = None,
  ):
    """Tries Records the audio in blocks, to be combined in post after pitch correction."""
    if instrument_allow_list is not None:
      filtered_schedule = self.filter_instruments(
        self.schedule, instrument_allow_list
      )
    else:
      filtered_schedule = self.schedule
    if transpose_override is None:
      transpose_override, _ = self.find_optimal_transpose(filtered_schedule)

    print(f"Transposing whole piece by {transpose_override}.")
    in_range_schedule, low_residue, high_residue = self.get_within_range_schedule(
      filtered_schedule,
      transpose_override,
    )

    high_residue_transpose, _ = self.find_optimal_transpose(high_residue)
    print(f"Transposing the notes too high by {high_residue_transpose}.")
    low_residue_transpose, _ = self.find_optimal_transpose(low_residue)
    print(f"Transposing the notes too low by {low_residue_transpose}.")

    self.maybe_initialize_record()
    track_id = 0
    for pass_id in range(total_passes[0]):
      self.reset_music_box()
      in_range_commands = self.schedule_to_command_list(
        in_range_schedule,
        transpose=transpose_override,
        total_passes=total_passes[0],
        pass_id=pass_id,
        tempo_multiplier=tempo_multiplier,
      )
      self.play_commands_chorded(
        in_range_commands,
        tempo_multiplier,
      )
      track_id += 1
    high_tracks = []
    for pass_id in range(total_passes[1]):
      self.reset_music_box()
      high_commands = self.schedule_to_command_list(
        high_residue,
        total_passes=total_passes[1],
        pass_id=pass_id,
        transpose=high_residue_transpose,
        tempo_multiplier=tempo_multiplier,
      )
      self.play_commands_chorded(
        high_commands,
        tempo_multiplier,
      )
      high_tracks.append(track_id)
      track_id += 1
    low_tracks = []
    for pass_id in range(total_passes[2]):
      self.reset_music_box()
      low_commands = self.schedule_to_command_list(
        low_residue,
        total_passes=total_passes[2],
        pass_id=pass_id,
        transpose=high_residue_transpose,
        tempo_multiplier=tempo_multiplier,
      )
      self.play_commands_chorded(
        low_commands,
        tempo_multiplier,
      )
      low_tracks.append(track_id)
      track_id += 1
    self.maybe_apply_audio_processing()

    high_pitch_correct = 100.0 * (
        2 ** ((transpose_override - high_residue_transpose) / 12) - 1
    )
    low_pitch_correct = 100.0 * (
        2 ** ((transpose_override - low_residue_transpose) / 12) - 1
    )
    for track in high_tracks:
      time.sleep(1)
      self.audacity.do_command(f"Select: Track={track} mode=Set")
      self.audacity.do_command("SelTrackStartToEnd")
      time.sleep(1)
      self.audacity.do_command(f"ChangePitch:Percentage={high_pitch_correct:.3f}")
    for track in low_tracks:
      self.audacity.do_command(f"Select: Track={track} mode=Set")
      self.audacity.do_command("SelTrackStartToEnd")
      time.sleep(1)
      self.audacity.do_command(f"ChangePitch:Percentage={low_pitch_correct:.3f}")
      time.sleep(1)


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


if __name__ == "__main__":
  config_path = "config.yaml"
  song = music_configs.empty_town
  midi_path = song.filename

  box_controller = MIDIMusicBoxController(config_path, midi_path, record=True)
  try:
    box_controller.play_with_transpose(
      tempo_multiplier=song.tempo_multiplier,
      default_high_transpose=song.default_high_transpose,
      transpose=song.transpose,
      instrument_allow_list=song.instrument_allow_list,
      total_passes=song.total_passes
      # mokou[0,1,2,4,6,8,10]# [0,1,2,4,6,10] 0.25-0.2
      # [3,4,5,6,13,14,15]#plants[0,1,2,3,4,5]
    )
  finally:
    print("Resetting...")
    box_controller.reset_music_box()
    time.sleep(1)
