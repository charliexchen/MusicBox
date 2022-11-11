"""Unfortunately, I started this codebase the day after the inciting incident which caused me to go completely
teetotal. The code is thereby hot garbage."""

import py_midicsv as pm
import threading
import time
import PyMidiBox
import yaml
from typing import Union, Tuple, Sequence

from typing import TypedDict

Note = dict[str, int]

Instrument = int


class MIDINote(TypedDict):
    ts: int
    note: int
    power: int


InstrumentSchedule = Sequence[MIDINote]

MIDDLE_C = 60
MIN_DELAY_MS = 200  # If a note is replayed within this time, then send the reset command immediately.
MAX_DELAY_MS = 5000  # Retract the note after this time


def parse_row(row: str) -> Union[Tuple[Instrument, MIDINote], bool]:
    """Parses a single row of Midi into note, timestamp and power."""
    cleaned_row = str.strip(row)
    arr = cleaned_row.split(", ")
    if arr[2] == "Note_on_c":
        note_data = {"ts": int(arr[1]), "note": int(arr[4]), "power": int(arr[5])}
        instrument = arr[3]
        return int(instrument), note_data
    return False


def parse_instrument(instrument_schedule: InstrumentSchedule, save_power: bool = False):
    per_note_schedule = {}
    for row in instrument_schedule:
        if row["note"] not in per_note_schedule:
            per_note_schedule[row["note"]] = []
        if save_power:
            per_note_schedule[row["note"]].append((row["ts"], row["power"]))
        else:
            if row["power"] > 0:
                per_note_schedule[row["note"]].append(row["ts"])
    return per_note_schedule


def mismatched_notes(
    instrument_schedules,
    valid_notes,
    transpose: int = 0,
) -> Tuple[int, int]:
    """Figures out how many mismatches notes there in the MIDI file. Returns tuple of the missed notes and missed notes
    in Teble section (which tends to have more effect on the melody）。"""
    mismatches = 0
    high_mismatch = 0
    for _instrument, instrument_schedule in instrument_schedules.items():
        for note in instrument_schedule:
            transposed_note = note + transpose
            if transposed_note not in valid_notes:
                if transposed_note > MIDDLE_C:
                    high_mismatch += len(instrument_schedule[note])
                mismatches += len(instrument_schedule[note])
    return mismatches, high_mismatch


def parse_midi(file_path: str):
    # Load the MIDI file and parse it into CSV format
    csv_string = pm.midi_to_csv(file_path)

    # Save the CSV for inspection purposes
    with open("inspection.csv", "w") as f:
        f.writelines(csv_string)

    # separate out the different instruments, so we can remove, say drums from the midi easily.
    # create a dict of instruments schedules, where each
    instrument_schedules = {}
    for row in csv_string:
        parsed_row = parse_row(row)
        if parsed_row:
            instrument, note_data = parsed_row
            if instrument not in instrument_schedules:
                instrument_schedules[instrument] = []
            instrument_schedules[instrument].append(note_data)
    # for each instrument,
    instrument_schedules = {
        instrument: parse_instrument(instrument_schedule)
        for instrument, instrument_schedule in instrument_schedules.items()
    }
    return instrument_schedules


class MIDIMusicBoxController(PyMidiBox.MusicBoxController):
    def __init__(self, config_path: str, midi_path: str):
        with open(config_path) as file:
            config = yaml.load(file, Loader=yaml.FullLoader)
        music_note_config = config["MUSIC_NOTE_CONFIG"]
        port = config["SERIAL_PORT"]
        baud_rate = config["BAUD_RATE"]
        super().__init__(music_note_config, port, baud_rate)
        self.instrument_schedules = parse_midi(midi_path)
        self.instruments = list(self.instrument_schedules.keys())
        self.transpose = self.find_transpose()
        self.schedule = None

    def find_transpose(self, scan_range=range(30, -30, -1), high_only=False) -> int:
        """Scans through transposes, and find the transposition which minimises the number of notes we have to remap."""
        min_transpose = None
        min_miss_count = float("inf")
        for transpose in scan_range:
            miss, high = mismatched_notes(
                self.instrument_schedules, self.music_note_config, transpose
            )
            if high_only:
                if high < min_miss_count:
                    min_miss_count = high
                    min_transpose = transpose
            else:
                if miss < min_miss_count:
                    min_miss_count = miss
                    min_transpose = transpose
            print(f"Transposing {transpose} results in {miss} misses.")
        print(
            f"Default transpose: {min_transpose} Semitones with {min_miss_count} misses."
        )
        return min_transpose

    def play_instrument(
        self,
        instrument_indices: Sequence[int] = None,
        tempo_multiplier: float = 1.0,
        transpose=None,
        total_passes: int = 1,
        pass_ind: int = 0,
        play_all: bool = False,
        play_residue=False,
    ):
        if instrument_indices is None:
            instrument_indices = self.instrument_schedules
            print(f"Playing all instruments {instrument_indices}")
        if transpose is None:
            transpose = self.transpose
        if self.schedule is None:
            self.schedule = get_instrument_schedule_all_notes(
                self.instrument_schedules,
                instrument_indices,
                tempo_multiplier,
                total_passes,
                pass_ind,
            )
            if not play_all:
                truncated_schedule, residue = self.get_within_range_schedule(transpose)
                if play_residue:
                    self.schedule = residue
                else:
                    self.schedule = truncated_schedule
        print(self.schedule)
        start_time = time.time()
        for note in self.schedule:
            scheduled_time = start_time + (note[0] / (tempo_multiplier * 1000)) + 1
            wait_time = scheduled_time - time.time()
            if wait_time > 0:
                time.sleep(wait_time)
            self.send_note_alpha(note[1] + transpose, note[2])

    def get_within_range_schedule(self, transpose=None):
        if transpose is None:
            transpose = self.transpose
        if self.schedule is None:
            self.schedule = get_instrument_schedule_all_notes(
                self.instrument_schedules,
                instrument_indices,
                tempo_multiplier,
                total_passes,
                pass_ind,
            )
        truncated_schedule = []
        residue = []
        for note in self.schedule:
            if note[1] + transpose in self.music_note_config:
                truncated_schedule.append(note)
            else:
                residue.append(note)
        return truncated_schedule, residue


def get_instrument_schedule_all_notes(
    instrument_schedules,
    instrument_indices: Sequence[int],
    tempo_multiplier: float = 1.0,
    total_passes: int = 1,
    pass_ind: int = 0,
) -> Sequence[Tuple[int, int, int]]:
    """Given a list of instruments, converts into a schedule of commands for the music box."""
    all_notes_schedule = []
    all_instrument_schedule = {}
    for instrument_index in instrument_indices:
        per_note_schedule = instrument_schedules[instrument_index]
        for note, times in per_note_schedule.items():
            if note not in all_instrument_schedule:
                all_instrument_schedule[note] = times
            else:
                all_instrument_schedule[note].extend(times)
    for note in all_instrument_schedule:
        times = [
            time
            for i, time in enumerate(sorted(all_instrument_schedule[note]))
            if i % total_passes == pass_ind
        ]
        if len(times) < 1:
            continue
        previous_note_time = times[0]
        all_notes_schedule.append((times[0], note, 1))
        for note_time in times[1:]:
            if note_time - previous_note_time < MIN_DELAY_MS * tempo_multiplier:
                all_notes_schedule.append((previous_note_time, note, 0))
                all_notes_schedule.append((note_time, note, 1))
            elif note_time - previous_note_time > MAX_DELAY_MS * tempo_multiplier:
                all_notes_schedule.append(
                    (previous_note_time + (MAX_DELAY_MS * tempo_multiplier), note, 0)
                )
                all_notes_schedule.append((note_time, note, 1))
            else:
                all_notes_schedule.append(
                    (note_time - (MIN_DELAY_MS * tempo_multiplier), note, 0)
                )
                all_notes_schedule.append((note_time, note, 1))
            previous_note_time = note_time
    # Sort all the commands for all keys
    all_notes_schedule.sort(key=lambda x: x[0])
    return all_notes_schedule


if __name__ == "__main__":
    config_path = "config.yaml"
    midi_path = "midis/chp_op18.mid"

    box_controller = MIDIMusicBoxController(config_path, midi_path)
    print("waiting...")
    time.sleep(8)
    print("Done!")
    box_controller.play_instrument(
        tempo_multiplier=1,
        transpose=None,
        total_passes=1,
        pass_ind=0,
    )
    time.sleep(60)
