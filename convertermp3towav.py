"""
   Converter mp3 files to Wav format
   Copyleft (c) 2023, Andres Sabas
"""
"""
   Functionality
   -------------
   Converter files from "audios" folder to .wav
"""
import os
from pydub import AudioSegment

path = "audios"
os.chdir(path)

audio_files = os.listdir()

for file in audio_files:
    name, ext = os.path.splitext(file)
    print(name)
    
    if ext == ".mp3":
        sound = AudioSegment.from_file(file, format="mp3",duration = 10)

        #Configure Mono Channel, Frame Rate 11025, 8 bits
        sound = sound.set_channels(1)
        sound = sound.set_frame_rate(11025)
        sound = sound.set_sample_width(1)

        # export
        file_handle = sound.export("{0}.wav".format(name), format='wav')

