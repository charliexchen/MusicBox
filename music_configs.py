from typing import Optional, List, NamedTuple


class config(NamedTuple):
  filename: str
  tempo_multiplier: float
  default_high_transpose: bool = False
  transpose: Optional[int] = None
  instrument_allow_list: Optional[List[int]] = None
  total_passes: int = 1


lemon = config(
  filename='midis/41063_Lemon--Kenshi-Yonezu.mid',
  tempo_multiplier=0.6
)


rain_falls = config(
  filename='midis/rain_falls_transcription.mid',
  tempo_multiplier=0.3
)

sparkle = config(
  filename='midis/Sparkle 2020 (Theishter).mid',
  tempo_multiplier = 2.5,
  total_passes = 2
)


nocturne = config(
  filename='midis/Nocturne-in-E-Flat-Opus-9-Nr-2.mid',
  tempo_multiplier = 0.8,
  total_passes = 1
)

yoru_ni_kakeru = config(
  filename='midis/YoruNiKakeru.mid',
  tempo_multiplier = 0.75,
  total_passes = 1
)

grande_waltz = config(
  filename='midis/chp_op18.mid',
  tempo_multiplier = 0.9,
  total_passes = 1
)

empty_town = config(
  filename='midis/Deltarune - Empty Town.mid',
  tempo_multiplier = 2,
  total_passes = 1
)
