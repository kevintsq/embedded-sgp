import sys
import math
import json
from threading import Thread

from PyQt5.QtChart import QValueAxis, QSplineSeries, QChart, QChartView, QPolarChart, QScatterSeries
from PyQt5.QtWidgets import QApplication, QWidget, QGridLayout, QLabel
from PyQt5.QtGui import QPainter, QImage, QPixmap
from PyQt5.QtCore import Qt, QTimer
import cv2 as cv

IPC_TYPE = "SOCKET"
PI2 = math.pi * 2

def run():
    if IPC_TYPE == "SOCKET":
        import socket

        HOST = ''
        PORT = 8080
        TRUNK_SIZE = 1024
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind((HOST, PORT))
            s.listen(1)
            connection, address = s.accept()
            with connection:
                print(f'Connected by {address}.')
                buffer = b''
                while True:
                    while b'\r' not in buffer:
                        data = connection.recv(TRUNK_SIZE)
                        if not data:
                            print(f"Connection closed from {address}.")
                            sys.exit(0)
                        buffer += data
                    data_list = buffer.split(b'\r')
                    last_one = data_list[-1]
                    if last_one == b'\n' or last_one == b'':
                        buffer = b''
                    else:
                        buffer = last_one
                    if len(data_list) > 1:
                        try:
                            result = json.loads(data_list[-2])
                        except:
                            print("Bad format.")
                            continue
                        if result["type"] == "tracks":
                            mainWindow.updateSoundSphere(result["src"])
                        elif result["type"] == "concentration":
                            mainWindow.updateCharts(result)
    elif IPC_TYPE == "PIPE":
        import subprocess

        with subprocess.Popen("./client", stdout=subprocess.PIPE, text=True) as p:
            while True:
                data = p.stdout.readline()
                if not data:
                    print("Connection closed.")
                    break
                print(data, end='')


class ChartView(QChartView):
    def __init__(self, titleName, titleUnit, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.setRenderHint(QPainter.Antialiasing)
        self.xAxis = QValueAxis()
        self.xAxis.setTitleText("Time (s)")
        self.xOriginal = 32
        self.xMax = self.xOriginal
        self.xAxis.setMax(self.xMax)
        self.setXaxisRange = False
        self.yAxis = QValueAxis()
        self.yAxis.setTitleText(f"{titleName} Concentration ({titleUnit})")
        self.yMax = 0
        self.yMin = 0
        self.firstTime = True
        self.series = QSplineSeries()
        self.series.setName(titleName)
        self.series.setUseOpenGL(True)
        self.chart = QChart()
        self.chart.addSeries(self.series)
        self.chart.addAxis(self.xAxis, Qt.AlignBottom)
        self.chart.addAxis(self.yAxis, Qt.AlignLeft)
        self.chart.setAnimationOptions(QChart.SeriesAnimations)  # AllAnimations
        # must be put after chart is available
        self.series.attachAxis(self.xAxis)
        self.series.attachAxis(self.yAxis)
        self.series.pointAdded.connect(self.redraw)
        self.setChart(self.chart)

    def redraw(self, index):
        y = self.series.at(index).y()
        if y > self.yMax:
            self.yMax = y
        if y < self.yMin:
            self.yMin = y
        elif self.firstTime:
            self.firstTime = False
            self.yMin = y
        self.yAxis.setRange(self.yMin - 2, self.yMax + 2)
        if self.series.count() > self.xOriginal:
            self.xMax += 1
            self.xAxis.setRange(self.xMax - self.xOriginal, self.xMax)


class PolarView(QChartView):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.setRenderHint(QPainter.Antialiasing)
        self.polarAxis = QValueAxis()
        self.points = QScatterSeries()
        self.points.setName("Sound Directions")
        self.points.setUseOpenGL(True)
        self.chart = QPolarChart()
        self.chart.addSeries(self.points)
        self.chart.addAxis(self.polarAxis, QPolarChart.PolarOrientationRadial)
        self.points.attachAxis(self.polarAxis)
        # self.points.append(-0.25, 0.5)
        # self.points.append(math.pi / 4, 0.5)
        # self.points.append(math.pi / 2, 0.5)
        # self.points.append(math.pi * 3 / 4, 0.5)
        # self.points.append(math.pi, 0.5)
        # self.points.append(math.pi * 5 / 4, 0.5)
        # self.points.append(math.pi * 3 / 2, 0.5)
        # self.points.append(math.pi * 7 / 4, 0.5)
        self.points.setPointLabelsVisible(True)
        self.setChart(self.chart)
        self.pointsCount = 0

    def updatePoints(self, data):
        self.points.clear()
        for i in data:
            if i["tag"] == "dynamic":
                angle = -math.atan2(i["y"], i["x"])
                if angle < 0:
                    angle += PI2
                self.points.append(angle / PI2, 0.75)


class CameraView(QLabel):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.setScaledContents(True)
        self.capture = cv.VideoCapture(0)
        self.timer = QTimer()
        self.timer.timeout.connect(self.captureAndDisplay)
        self.timer.start(16)

    def captureAndDisplay(self):
        _, data = self.capture.read()
        self.image = cv.cvtColor(data, cv.COLOR_BGR2RGB)
        self.setPixmap(QPixmap.fromImage(QImage(self.image.data, self.image.shape[1], self.image.shape[0], QImage.Format_RGB888).scaled(mainWindow.width() // 3, mainWindow.height() // 2, Qt.KeepAspectRatio)))


class MainWindow(QWidget):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.setWindowTitle("Gas Concentration")
        self.ethanolChartView = ChartView("Ethanol", "raw")
        self.h2ChartView = ChartView("H2", "raw")
        self.tVocCharView = ChartView("tVOC", "ppb")
        self.co2ChartView = ChartView("CO2eq", "ppm")
        self.cameraView = CameraView()
        self.soundDirectionView = PolarView()

        layout = QGridLayout()
        layout.addWidget(self.ethanolChartView, 0, 0)
        layout.addWidget(self.h2ChartView, 0, 1)
        layout.addWidget(self.cameraView, 0, 2)
        layout.addWidget(self.tVocCharView, 1, 0)
        layout.addWidget(self.co2ChartView, 1, 1)
        layout.addWidget(self.soundDirectionView, 1, 2)
        self.setLayout(layout)

    def updateCharts(self, data):
        timeStamp = data["timeStamp"]
        self.ethanolChartView.series.append(timeStamp, data["Ethanol"])
        self.h2ChartView.series.append(timeStamp, data["H2"])
        self.tVocCharView.series.append(timeStamp, data["tVOC"])
        self.co2ChartView.series.append(timeStamp, data["CO2"])

    def updateSoundSphere(self, data):
        self.soundDirectionView.updatePoints(data)


if __name__ == "__main__":
    QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
    QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps, True)
    app = QApplication(sys.argv)
    app.beep()
    mainWindow = MainWindow()
    model = Thread(target=run, args=(), daemon=True)
    model.start()
    mainWindow.show()
    sys.exit(app.exec())
