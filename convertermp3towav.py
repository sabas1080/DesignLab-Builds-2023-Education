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
from humanize import naturalsize

path = "audios"
os.chdir(path)

audio_files = os.listdir()

for file in audio_files:
    name, ext = os.path.splitext(file)
    print(name)

    size = os.stat(file).st_size
    print(size)

    if ext == ".m4a":
        print("Converter m4a to wav")
        sound = AudioSegment.from_file(file, format="m4a", duration=10)
        sound = sound + 10
        # Configure Mono Channel, Frame Rate 11025, 8 bits
        sound = sound.set_channels(1)
        sound = sound.set_frame_rate(11025)
        sound = sound.set_sample_width(1)

        # export
        file_handle = sound.export("{0}.wav".format(name), format="wav")

        size = os.stat("{0}.wav".format(name)).st_size
        print(naturalsize(size))
        if size > 31000:
            print("'ALERTA: Es un archivo demasiado grande:'{0}.wav".format(name))
    if ext == ".mp3":
        print("Converter mp3 to wav")
        sound = AudioSegment.from_file(file, format="mp3", duration=10)
        sound = sound + 10
        # Configure Mono Channel, Frame Rate 11025, 8 bits
        sound = sound.set_channels(1)
        sound = sound.set_frame_rate(11025)
        sound = sound.set_sample_width(1)

        # export
        file_handle = sound.export("{0}.wav".format(name), format="wav")

        size = os.stat("{0}.wav".format(name)).st_size
        print(naturalsize(size))
        if size > 31000:
            print("'ALERTA: Es un archivo demasiado grande:'{0}.wav".format(name))
    if ext == ".wav":
        print("Converter wav to wav")
        sound = AudioSegment.from_file(file, format="wav", duration=10)
        sound = sound + 10
        # Configure Mono Channel, Frame Rate 11025, 8 bits
        sound = sound.set_channels(1)
        sound = sound.set_frame_rate(11025)
        sound = sound.set_sample_width(1)

        # export
        file_handle = sound.export("{0}.wav".format(name), format="wav")

        size = os.stat("{0}.wav".format(name)).st_size
        print(naturalsize(size))
        if size > 31000:
            print("'ALERTA: Es un archivo demasiado grande:'{0}.wav".format(name))
