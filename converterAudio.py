# SPDX-FileCopyrightText: Andres Sabas @ DesignLab
#
# SPDX-License-Identifier: MIT

import os.path
from pydub import AudioSegment
from pydub.playback import play
import PySimpleGUI as sg
from humanize import naturalsize


font = ('Courier New', 11)
sg.theme('DarkBlue3')
sg.set_options(font=font)

layout = [
    [
        sg.Text("Image Folder"),
        sg.In(size=(25, 1), enable_events=True, key="-FOLDER-"),
        sg.FolderBrowse(),
        sg.Button('Converter'), 
        sg.Button('Exit'),
    ],
    [
        sg.Listbox(
            values=[], enable_events=True, size=(40, 20), key="-FILE LIST-"
        )
    ],
]

window = sg.Window("Converter MP3 to WAV", layout, finalize=True)
song = None
while True:

    event, values = window.read()

    if event in (sg.WINDOW_CLOSED, 'Exit'):
        break
    elif event == '-FOLDER-':
        folder = values['-FOLDER-']
        try:
            # Get list of files in folder
            audio_files = os.listdir(folder)
        except:
                audio_files = []
        fnames = [
            f
            for f in audio_files
            if os.path.isfile(os.path.join(folder, f))
            and f.lower().endswith((".mp3", ".wav"))
        ]
        window["-FILE LIST-"].update(fnames)
    elif event == 'Converter':
        if audio_files:
            for file in audio_files:
                name, ext = os.path.splitext(file)
                print(name)

                file = os.path.join(
                values["-FOLDER-"], file
            )
    
                size = os.stat(file).st_size
                print(size)
    
                if ext == ".m4a":
                    print('Converter m4a to wav') 
                    sound = AudioSegment.from_file(file, format="m4a",duration = 10)
                    sound = sound + 10
                    #Configure Mono Channel, Frame Rate 11025, 8 bits
                    sound = sound.set_channels(1)
                    sound = sound.set_frame_rate(11025)
                    sound = sound.set_sample_width(1)
        
                    # export
                    file_handle = sound.export("{0}.wav".format(name), format='wav')
        
                    size = os.stat("{0}.wav".format(name)).st_size
                    #print(naturalsize(size))
                    if(size > 31000):
                        print("'ALERTA: Es un archivo demasiado grande:'{0}.wav".format(name) )   
                if ext == ".mp3":
                    print('Converter mp3 to wav')
                    sound = AudioSegment.from_file(file, format="mp3",duration = 10)
                    sound = sound + 10
                    #Configure Mono Channel, Frame Rate 11025, 8 bits
                    sound = sound.set_channels(1)
                    sound = sound.set_frame_rate(11025)
                    sound = sound.set_sample_width(1)

                    # export
                    file_handle = sound.export("{0}.wav".format(name), format='wav')
        
                    size = os.stat("{0}.wav".format(name)).st_size
                    #print(naturalsize(size))
                    if(size > 31000):
                        print("'ALERTA: Es un archivo demasiado grande:'{0}.wav".format(name) )    
                if ext == ".wav":
                    print('Converter wav to wav')
                    sound = AudioSegment.from_file(file, format="wav",duration = 10)
                    sound = sound + 10
                    #Configure Mono Channel, Frame Rate 11025, 8 bits
                    sound = sound.set_channels(1)
                    sound = sound.set_frame_rate(11025)
                    sound = sound.set_sample_width(1)

                    # export
                    file_handle = sound.export("{0}.wav".format(name), format='wav')
        
                    size = os.stat("{0}.wav".format(name)).st_size
                    #print(naturalsize(size))
                    if(size > 31000):
                        print("'ALERTA: Es un archivo demasiado grande:'{0}.wav".format(name) )   
        print("Conversion Terminada" )   

window.close()