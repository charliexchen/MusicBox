from tkinter import Tk, Scale, HORIZONTAL, Button
import pickle
from functools import partial
import serial

MAX = 2200
MIN = 800
DEFAULT_MIN = 1000
DEFAULT_MAX = 2000
SERVO_COUNT = 30
root = Tk()
root.title("Servo Calibration")

serial_conn = serial.Serial("COM13", baudrate=9600)


def update_servos(servo_id: int, position: int) -> bytes:
    """
    Converts a servo command pair into two bytes, which can then be sent to the robot
    :param servo_id
    :param position: servo command based on PWM
    :return: two bytes which the arduino can parse and act on.
    """

    shift_command_id = 0
    shift_position = 1500 + (servo_id - (servo_id % 8))
    shift_command = (shift_position << 3) + shift_command_id
    second_byte = (shift_command & 127) | 128
    first_byte = (shift_command >> 7) & 127
    serial_conn.write(bytes([first_byte, second_byte]))

    discretized_position = int(position) - 800
    command = (discretized_position << 3) + servo_id % 8
    assert command >> 14 == 0, "Error command exceeds 16 bits"
    second_byte = (command & 127) | 128
    first_byte = (command >> 7) & 127
    serial_conn.write(bytes([first_byte, second_byte]))
    print("sending: " + str(position))


sliders = [
    Scale(
        root,
        from_=MIN,
        to=MAX,
        orient=HORIZONTAL,
        sliderlength=10,
        length=800,
        width=10,
        command=partial(update_servos, id),
    )
    for id in range(SERVO_COUNT)
]

default_slider_val = [1489, 1241, 1541, 1374, 1472, 1342, 1484, 1383, 1434, 1351, 1374, 1429, 1496, 1342, 1448, 1402, 1537, 1248, 1500, 1306, 1520, 1276, 1459, 1379, 1562, 1422, 1498, 1335, 1377, 1386]
for i, slider in enumerate(sliders):

    slider.set(default_slider_val[i])

    slider.pack()


def export_data():
    slider_data = [slider.get() for slider in sliders]
    pickle.dump(slider_data, open("data.p", "wb"))
    print(slider_data)


def send_spin_command():
    servo_id = 0
    discretized_position = 1564
    command = (discretized_position << 3) + servo_id
    second_byte = (command & 127) | 128
    first_byte = (command >> 7) & 127
    serial_conn.write(bytes([first_byte, second_byte]))


export_button = Button(root, text="Export Settings", command=export_data)
export_button.pack()
spin_button = Button(root, text="Spin!", command=send_spin_command)
spin_button.pack()
root.mainloop()
