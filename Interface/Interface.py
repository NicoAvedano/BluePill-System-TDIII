# Agregamos un botón de "Reset UART"
# que limpia el buffer y reinicia variables

import sys
import serial
import struct
from PyQt5.QtWidgets import (QApplication, QWidget, QVBoxLayout, QLabel, QSlider,
                             QPushButton, QHBoxLayout, QGridLayout)
from PyQt5.QtCore import Qt, QTimer
import pyqtgraph as pg

# --- Puerto Serie ---
ser = serial.Serial('COM5', 115200, timeout=0.1)

def crc_simple(data):
    chk = 0
    for b in data:
        chk |= b
    return chk

class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Interface STM32")

        self.tx_pwm1 = 0
        self.tx_pwm2 = 0
        self.tx_digital = 0

        self.rx_data = bytearray(16)
        self.adc_labels = []
        self.in_labels = []

        self.temp_value_label = QLabel("Temperatura: -- °C")
        self.press_value_label = QLabel("Presion: -- hPa")


        self.temp_data = []
        self.press_data = []

        self.init_ui()

        self.temp_curve = self.temp_plot.plot(pen='r')
        self.press_curve = self.press_plot.plot(pen='b')

        self.timer = QTimer()
        self.timer.timeout.connect(self.read_serial)
        self.timer.start(100)

    def init_ui(self):
        layout = QVBoxLayout()

        self.rx_label = QLabel("Trama RX:")
        layout.addWidget(self.rx_label)

        # --- Botón Reset UART ---
        reset_button = QPushButton("Reset UART")
        reset_button.clicked.connect(self.reset_uart)
        layout.addWidget(reset_button)

        pwm_layout = QGridLayout()
        self.pwm1_slider = QSlider(Qt.Horizontal)
        self.pwm1_slider.setRange(0, 100)
        self.pwm1_slider.valueChanged.connect(self.send_command)
        pwm_layout.addWidget(QLabel("PWM1"), 0, 0)
        pwm_layout.addWidget(self.pwm1_slider, 0, 1)

        self.pwm2_slider = QSlider(Qt.Horizontal)
        self.pwm2_slider.setRange(0, 100)
        self.pwm2_slider.valueChanged.connect(self.send_command)
        pwm_layout.addWidget(QLabel("PWM2"), 1, 0)
        pwm_layout.addWidget(self.pwm2_slider, 1, 1)
        layout.addLayout(pwm_layout)

        digital_layout = QHBoxLayout()
        self.digital_buttons = []
        for i in range(4):
            btn = QPushButton(f"OUT{i+1}")
            btn.setCheckable(True)
            btn.toggled.connect(self.update_digital)
            digital_layout.addWidget(btn)
            self.digital_buttons.append(btn)
        layout.addLayout(digital_layout)

        entrada_layout = QHBoxLayout()
        for i in range(3):
            label = QLabel(f"IN{i+1}: 0")
            entrada_layout.addWidget(label)
            self.in_labels.append(label)
        layout.addLayout(entrada_layout)

        adc_layout = QVBoxLayout()
        for i in range(4):
            label = QLabel(f"ADC{i+1}: 0")
            adc_layout.addWidget(label)
            self.adc_labels.append(label)
        layout.addLayout(adc_layout)

        self.temp_plot = pg.PlotWidget(title="Temperatura (C)")
        self.press_plot = pg.PlotWidget(title="Presion (hPa)")

        layout.addWidget(self.temp_value_label)
        layout.addWidget(self.press_value_label)


        layout.addWidget(self.temp_plot)
        layout.addWidget(self.press_plot)

        self.setLayout(layout)

    def update_digital(self):
        self.tx_digital = 0
        for i, btn in enumerate(self.digital_buttons):
            if btn.isChecked():
                self.tx_digital |= (1 << i)
        self.send_command()

    def send_command(self):
        pwm1 = int(self.pwm1_slider.value() * 4095 / 100)
        pwm2 = int(self.pwm2_slider.value() * 4095 / 100)
        out = self.tx_digital & 0x0F
        data = [0xA5,
                (pwm1 >> 4) & 0xFF,
                ((pwm1 & 0xF) << 4) | ((pwm2 >> 8) & 0xF),
                pwm2 & 0xFF,
                out,
                0]
        data[5] = crc_simple(data[:5])
        ser.write(bytearray(data))

    def read_serial(self):
        if ser.in_waiting >= 16:
            self.rx_data = ser.read(16)
            self.update_display()

    def update_display(self):
        hex_str = ' '.join(f"{b:02X}" for b in self.rx_data)
        self.rx_label.setText(f"Trama RX: {hex_str}")

        pwm1 = ((self.rx_data[1] << 4) | (self.rx_data[2] >> 4))
        pwm2 = (((self.rx_data[2] & 0x0F) << 8) | self.rx_data[3])
        entradas = self.rx_data[4] & 0x0F
        for i in range(3):
            self.in_labels[i].setText(f"IN{i+1}: {(entradas >> i) & 1}")

        adc_vals = [
            ((self.rx_data[5] << 4) | (self.rx_data[6] >> 4)),
            (((self.rx_data[6] & 0x0F) << 8) | self.rx_data[7]),
            ((self.rx_data[8] << 4) | (self.rx_data[9] >> 4)),
            (((self.rx_data[9] & 0x0F) << 8) | self.rx_data[10])
        ]
        for i in range(4):
            self.adc_labels[i].setText(f"ADC{i+1}: {adc_vals[i]}")

        temp = ((self.rx_data[11] << 8) | self.rx_data[12]) / 100
        pres = ((self.rx_data[13] << 8) | self.rx_data[14])

        self.temp_value_label.setText(f"Temperatura: {temp:.2f} °C")
        self.press_value_label.setText(f"Presion: {pres} hPa")

        self.temp_data.append(temp)
        self.press_data.append(pres)
        self.temp_curve.setData(self.temp_data[-100:])
        self.press_curve.setData(self.press_data[-100:])

    def reset_uart(self):
        ser.reset_input_buffer()
        self.rx_data = bytearray(16)
        self.rx_label.setText("Trama RX: --")
        self.temp_data.clear()
        self.press_data.clear()
        self.temp_curve.clear()
        self.press_curve.clear()

app = QApplication(sys.argv)
window = MainWindow()
window.show()
sys.exit(app.exec_())
