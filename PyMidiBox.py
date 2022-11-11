import sys
import time
import yaml
import serial
from typing import TypedDict

from rtmidi.midiutil import open_midiinput

MIDDLE_C = 60
OCTAVE = 12
CAPTIAL_A = 65
LOWER_CASE_A = 97


class NoteInfo(TypedDict):
    offset: int
    id: int


class MusicBoxController:
    """Sends data to the servoController and handles remapping of notes"""

    def __init__(
        self,
        music_note_config: dict[int, NoteInfo],
        port: int,
        baudrate: int,
        transpose=0,
    ):
        self.serial_conn = serial.Serial(port, baudrate=baudrate)
        self.music_note_config = music_note_config
        self._transpose = transpose

    def remap_to_music_box_range(self, note: int) -> int:
        """Unfortunately, not all the notes in are available on the music box, and the music box is only chromatic on
        the middle portion. This function maps any notes outside of the range to the same note on a different octave
        which is available on the music box."""
        if note in self.music_note_config:
            return note
        init_note = note
        if note > MIDDLE_C:
            while note not in self.music_note_config:
                note -= OCTAVE
        else:
            while note not in self.music_note_config:
                note += OCTAVE
        return note

    def send_note(self, note: int, stepper: bool = True, power: int = -1):
        """Sends one or two bytes. The first tells the arduino which servo should be moved, and the second one is a
        seven bit measure of the force the note was pushed down. It is not clear if the latter will actually be useful
        on a music box, so this is optional. power > 0 denotes how hard the note was pressed down. power = 0 denotes the
        note was released (as per convention from the midi libraries I'm using) and -1 means we don't send the second
        byte."""
        if stepper:
            remapped_note_command = note
        else:
            remapped_note = self.remap_to_music_box_range(note + self._transpose)
            remapped_note_command = self.music_note_config[remapped_note]["id"]

        self.serial_conn.write(bytes([remapped_note_command]))
        if power >= 0:
            power_command = 128 + power
            self.serial_conn.write(power_command)

    def send_note_alpha(self, note: int, power: int, log=False):
        """sends a byte in alphabet range to make debugging easier"""
        if log:
            print(f"Playing note {note}")
        remapped_note = self.remap_to_music_box_range(note + self._transpose)
        if power == 0:
            remapped_note_command = (
                self.music_note_config[remapped_note]["id"] + CAPTIAL_A
            )
            if note == 108:
                self._transpose += 1
                if log:
                    print(f"Transposing notes by {self._transpose}")
            if note == 107:
                self._transpose -= 1
                if log:
                    print(f"Transposing notes by {self._transpose}")
        else:
            remapped_note_command = (
                self.music_note_config[remapped_note]["id"] + LOWER_CASE_A
            )
        self.serial_conn.write(bytes([remapped_note_command]))

    def reset_music_box(self):
        self._transpose = 0
        for note in self.music_note_config:
            self.send_note_alpha(note, 0)
            time.sleep(0.03)

    def hard_reset_music_box(self):
        self._transpose = 0
        for note in self.music_note_config:
            self.send_note_alpha(note, 1)
            time.sleep(0.03)
        self.reset_music_box()

    def send_calibration_data(self, calibration_data):
        """Sends down a command to initiate calibration mode in the servo, sends down calibration data and then sends
        command to exit calibration mode."""

    def test_all_notes_command(self):
        """simple command which loops over all the available music box notes in order to make sure that pipes are
        working end to end."""
        for note in sorted(self.music_note_config.keys()):
            self.send_note(note)
            time.sleep(0.2)


class MIDIMusicBox:
    """Listens for and records MIDI events from MIDI setups (e.g. my keyboard)"""

    def __init__(self, config_path: str):
        """Prompt user to select the MIDI Device from list if not defined"""

        with open(config_path) as file:
            config = yaml.load(file, Loader=yaml.FullLoader)
        keyboard_port = config["KEYBOARD_PORT"]
        try:
            self.midi_in, self.port_name = open_midiinput(keyboard_port)
            print(self.port_name)
        except (EOFError, KeyboardInterrupt):
            sys.exit()

        music_note_config = config["MUSIC_NOTE_CONFIG"]
        port = config["SERIAL_PORT"]
        baud_rate = config["BAUD_RATE"]
        self.music_box_controller = MusicBoxController(
            music_note_config, port, baud_rate
        )
        time.sleep(5)
        self.music_box_controller.test_all_notes_command()

    def listen_loop(self):
        """Based off the example from rt midi. Starts a loop which listens for events from the midi device and pushes it
        to a queue to be consumed by the servo controller."""
        try:
            t = 0
            while True:
                data = self.midi_in.get_message()
                if data:
                    message, deltatime = data
                    t += deltatime
                    if message[0] == 144:  # 144 means the message is a note
                        note = message[1]
                        power = message[2]
                        self.music_box_controller.send_note_alpha(note, power)
        except KeyboardInterrupt:
            print("")
        finally:
            print("Exit.")
            self.midi_in.close_port()
            del self.midi_in

    def keyboard_test(self):
        import pygame

        pygame.init()
        pygame.display.set_caption("Quick Start")
        window_surface = pygame.display.set_mode((800, 600))
        background = pygame.Surface((800, 600))
        background.fill(pygame.Color("#ffffff"))
        is_running = True
        clock = pygame.time.Clock()
        while is_running:
            clock.tick(60)
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    is_running = False
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_a:
                        self.music_box_controller.send_note(41)
                    elif event.key == pygame.K_s:
                        self.music_box_controller.send_note(66)
            window_surface.blit(background, (0, 0))
            pygame.display.update()


if __name__ == "__main__":
    path = "config.yaml"
    music_box = MIDIMusicBox(path)
    music_box.listen_loop()
