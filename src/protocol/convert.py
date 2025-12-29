import io
import sys

from pydub import AudioSegment

import protocol


def convert_audio_to_wav_pcm(file_path):
    """
    Convert any audio file to mono 16-bit 22000Hz WAV and return as BytesIO.

    Args:
        file_path (str): path to input audio file

    Returns:
        BytesIO: WAV audio as a file-like object
    """
    # Load the input audio from path
    audio = AudioSegment.from_file(file_path)

    # Convert to mono, 16-bit, 22000 Hz
    audio = audio.set_channels(1).set_frame_rate(22000).set_sample_width(2)

    # Export into a BytesIO buffer
    buffer = io.BytesIO()
    audio.export(buffer, format="wav")
    buffer.seek(0)

    return buffer


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("\033[1;31mUsage: python convert.py <input_file> [remote_name]\033[0m")
        sys.exit(1)
    elif len(sys.argv) > 3:
        print("\033[1;31mToo many arguments\033[0m")
        sys.exit(1)

    input_path = sys.argv[1]
    remote_name = sys.argv[2] if len(sys.argv) == 3 else "audio.wav"

    device = protocol.ProtocolClient()

    wav_buffer = convert_audio_to_wav_pcm(input_path)

    size = wav_buffer.getbuffer().nbytes

    def update_progress(transferred, speed):
        print(
            f"\033[33mTransferred: {transferred / size * 100:.2f} %, Speed: {speed:.2f} KB/s\033[0m",
            end="\r",
        )

    device.push(wav_buffer, size, remote_name, update_progress)
