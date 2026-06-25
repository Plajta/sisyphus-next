from dataclasses import dataclass
from io import BytesIO
from statistics import mean

from protocol import MeasuredColor, ProtocolClient


@dataclass
class ColorInfo:
    name: str
    code: str
    representation: int
    ansi_color: str = ""

    def __post_init__(self):
        if len(self.code) != 1:
            raise ValueError("color_code must be a single character")


colorinfo_list = [
    ColorInfo(
        name="red (1)", code="r", representation=0xAA0000, ansi_color="\033[0;31m"
    ),
    ColorInfo(
        name="yellow (2)", code="y", representation=0xAAAA00, ansi_color="\033[0;33m"
    ),
    ColorInfo(
        name="green (3)", code="g", representation=0x00AA00, ansi_color="\033[0;32m"
    ),
    ColorInfo(
        name="cyan (4)", code="c", representation=0x00AAAA, ansi_color="\033[0;36m"
    ),
    ColorInfo(
        name="blue (5)", code="b", representation=0x0000AA, ansi_color="\033[0;34m"
    ),
    ColorInfo(
        name="magenta (6)", code="m", representation=0xAA00AA, ansi_color="\033[0;35m"
    ),
    ColorInfo(name="white (7)", code="w", representation=0x555555, ansi_color=""),
]


@dataclass
class Color:
    info: ColorInfo
    measured: MeasuredColor


if __name__ == "__main__":
    device = ProtocolClient()

    colors = []

    for color in colorinfo_list:
        print(
            f"Please put the {color.ansi_color}{color.name}\033[0m paper in the device and press \033[1mEnter\033[0m..."
        )
        input()

        measurements = []
        for i in range(5):
            measurements.append(device.measure())

        averaged = MeasuredColor(
            hue=round(mean(c.hue for c in measurements)),
            saturation=round(mean(c.saturation for c in measurements)),
            value=round(mean(c.value for c in measurements)),
            clear=round(mean(c.clear for c in measurements)),
        )

        colors.append(Color(info=color, measured=averaged))

    buf = BytesIO()

    for color in colors:
        buf.write(
            f"{color.measured.hue} {color.measured.saturation} {color.measured.value} {color.info.code} {color.info.representation:06X}\n".encode(
                "ascii"
            )
        )

    buf_size = len(buf.getvalue())
    buf.seek(0)

    def update_progress(transferred, speed):
        print(
            f"\033[33mTransferred: {transferred / buf_size * 100:.2f} %, Speed: {speed:.2f} KB/s\033[0m",
            end="\r",
        )

    device.push(buf, buf_size, "color_lookup_table", update_progress)
    print()
    print("\033[32mDone!\033[0m")
