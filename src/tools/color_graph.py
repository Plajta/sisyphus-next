'''
Disclaimer: This code was partially written by an LLM

It works as a visualiser for color data so we can calibrate better.
'''

import sys
import protocol

from PyQt6.QtWidgets import QApplication, QMainWindow, QPushButton, QVBoxLayout, QWidget, QHBoxLayout, QLabel
from PyQt6.QtCharts import QChart, QChartView, QLineSeries, QValueAxis
from PyQt6.QtGui import QPainter, QColor, QPen
from PyQt6.QtCore import Qt

device = protocol.ProtocolClient()

# A simple dark theme stylesheet
DARK_STYLESHEET = """
QWidget {
    background-color: #2b2b2b;
    color: #f0f0f0;
    font-size: 10pt;
}
QMainWindow {
    border: 1px solid #1e1e1e;
}
QPushButton {
    background-color: #4a4a4a;
    border: 1px solid #555555;
    padding: 6px;
    border-radius: 3px;
}
QPushButton:hover {
    background-color: #5a5a5a;
}
QPushButton:pressed {
    background-color: #3a3a3a;
}
"""

class ColorPreview(QWidget):
    """A small widget that shows the current color in HSV space."""
    def __init__(self):
        super().__init__()
        self.setFixedSize(80, 50)  # Small rectangle
        self.color = QColor(0, 0, 0)  # Default black

    def set_color(self, h, s, v):
        """Update the displayed color from HSV values (h:0-360, s/v:0-100)."""
        self.color.setHsvF(h/360.0, s/100.0, v/100.0)
        self.update()  # Trigger repaint

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.fillRect(self.rect(), self.color)

class ColorIntensityApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Color measurement doohickey")
        self.setMinimumSize(800, 600)

        # --- Chart Setup ---
        self.chart = QChart()
        self.chart.setTheme(QChart.ChartTheme.ChartThemeDark)
        self.chart.setTitle("Color measurements (HSV)")

        # --- Data Series ---
        self.hue_series = QLineSeries()
        self.hue_series.setName("Hue")
        self.hue_series.setColor(QColor("#ff55ff"))

        self.saturation_series = QLineSeries()
        self.saturation_series.setName("Saturation")
        self.saturation_series.setColor(QColor("#ffff55"))

        self.value_series = QLineSeries()
        self.value_series.setName("Value")
        self.value_series.setColor(QColor("#55ffff"))

        self.clear_series = QLineSeries()
        self.clear_series.setName("Clear")
        clear_pen = QPen(QColor("#aaaaaa"))
        clear_pen.setStyle(Qt.PenStyle.DashLine)
        self.clear_series.setPen(clear_pen)

        for series in (self.hue_series, self.saturation_series, self.value_series, self.clear_series):
            self.chart.addSeries(series)

        # --- Axes (Manual Creation) ---
        self.axis_x = QValueAxis()
        self.axis_x.setTitleText("Measurement #")
        # Start axis at 1 to align first point, range 1–10
        self.axis_x.setRange(1, 10)
        self.axis_x.setLabelFormat("%d")
        self.chart.addAxis(self.axis_x, Qt.AlignmentFlag.AlignBottom)

        self.axis_y = QValueAxis()
        self.axis_y.setRange(0, 360)
        self.chart.addAxis(self.axis_y, Qt.AlignmentFlag.AlignLeft)

        for series in (self.hue_series, self.saturation_series, self.value_series, self.clear_series):
            series.attachAxis(self.axis_x)
            series.attachAxis(self.axis_y)

        # --- Chart View ---
        self.chart_view = QChartView(self.chart)
        self.chart_view.setRenderHint(QPainter.RenderHint.Antialiasing)
        self.measurement_count = 0

        # --- HSV Preview Widget ---
        self.color_preview = ColorPreview()
        self.hsv_label = QLabel("Clear: --  H: --  S: --  V: --");

        # --- UI Controls ---
        self.new_button = QPushButton("New Measurement")
        self.reset_button = QPushButton("Reset")
        self.new_button.clicked.connect(self.add_measurement)
        self.reset_button.clicked.connect(self.reset_plot)

        # --- Layout ---
        button_layout = QHBoxLayout()
        button_layout.addWidget(self.new_button)
        button_layout.addWidget(self.reset_button)
        button_layout.addStretch()
        button_layout.addWidget(self.hsv_label)
        button_layout.addWidget(self.color_preview)

        main_layout = QVBoxLayout()
        main_layout.addWidget(self.chart_view)
        main_layout.addLayout(button_layout)

        container = QWidget()
        container.setLayout(main_layout)
        self.setCentralWidget(container)

    def add_measurement(self):
        """Generates new random data points and adds them to the series."""
        self.measurement_count += 1

        color = device.measure()

        self.hue_series.append(self.measurement_count, color.hue)
        self.saturation_series.append(self.measurement_count, color.saturation)
        self.value_series.append(self.measurement_count, color.value)
        self.clear_series.append(self.measurement_count, color.clear)

        self.color_preview.set_color(color.hue, color.saturation, color.value)
        self.hsv_label.setText(f"Clear: {color.clear}  H: {color.hue:.1f}°  S: {color.saturation:.1f}%  V: {color.value:.1f}%")

        # Always grow the X-axis max (Qt ignores shrinking below current max)
        self.axis_x.setMax(self.measurement_count)

    def reset_plot(self):
        """Clears all data from the series and resets the view."""
        for series in (self.hue_series, self.saturation_series, self.value_series, self.clear_series):
            series.clear()

        self.measurement_count = 0
        # Reset axis back to 1–10
        self.axis_x.setRange(1, 10)

if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setStyleSheet(DARK_STYLESHEET)
    window = ColorIntensityApp()
    window.show()
    sys.exit(app.exec())
