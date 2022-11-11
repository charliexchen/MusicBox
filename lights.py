#!/usr/bin/env python3
from openrgb import OpenRGBClient
from openrgb.utils import RGBColor, DeviceType
import time

client = OpenRGBClient()

print(client)

print(client.devices)
RAM_ORDERING = [1, 3, 0, 2]
FAN_SHIFT = [1,10,10,5,8,7]
PER_FAN_RGB_COUNT = 12
FAN_COUNT = 6

ram_grid = []
for id in RAM_ORDERING:
  ram_grid.append(client.devices[id].leds)

motherboard = client.get_devices_by_type(DeviceType.MOTHERBOARD)[0]
print(motherboard.leds)
motherboard_main_argb = [led for led in motherboard.leds if 'Aura Mainboard' in led.name]
argb = [led for led in motherboard.leds if 'Aura Addressable 1' in led.name]
aura_rgb = motherboard_main_argb[-2:]
motherboard_main_argb = motherboard_main_argb[:-2]
fan_argb = []
for i in range(72):
  if i % 12 == 0:
    fan_argb.append([])
  fan_argb[-1].append(argb[i])

def apply_uniform(colour, leds):
  if isinstance(leds, list):
    for led in leds:
      apply_uniform(colour, led)
  else:
    leds.set_color(colour)

for i, hue in enumerate(range(330, 120, -35)):
  print(hue)
  apply_uniform(RGBColor.fromHSV(hue,100,100), fan_argb[i])
  time.sleep(0.3)
aura_rgb[1].set_color(RGBColor.fromHSV(300,100,100))
aura_rgb[0].set_color(RGBColor.fromHSV(180,100,100))


apply_uniform(RGBColor.fromHSV(15,100,100), ram_grid[0])
time.sleep(0.3)
apply_uniform(RGBColor.fromHSV(15,100,100), ram_grid[2])
time.sleep(0.3)
apply_uniform(RGBColor.fromHSV(0,100,100), ram_grid[1])
time.sleep(0.3)
apply_uniform(RGBColor.fromHSV(0,100,100), ram_grid[3])
time.sleep(0.3)

staggered = [0,11,1,10,2,9,3,8,4,7,5,6]
for i in range(FAN_COUNT):
  for j in range(PER_FAN_RGB_COUNT):
    fan_argb[i][(staggered[j]+FAN_SHIFT[i])%12].set_color(RGBColor.fromHSV(0,100,100))
    if i%2 ==0:
      time.sleep(0.01)