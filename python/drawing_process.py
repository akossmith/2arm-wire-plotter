from threading import Thread, Event
from queue import Queue

from printer_commander import *
from gcodehandler import *


class DrawingProcess(Thread):
    def __init__(self,
                 printer: PrinterCommander,
                 filename: str,
                 interpolation_resolution: float = 0.1,
                 speed: float = 300):
        super().__init__()
        self.stop_event = Event()
        self.printer = printer
        self.drawn_points = Queue()  # todo: make this exist in main thread (bug: last segment not displayed on screen)
        self.interpolator = GCodeInterpolator(read_gcode_file(filename),
                                              max_point_distance_mm=interpolation_resolution)
        self.speed = speed
        self.printing_method = self.burst_printing

    def stop(self):
        self.stop_event.set()

    def run(self):
        self.printer.set_rpm(self.speed)
        self.printing_method()

    def regular_printing(self):
        for x, y in self.interpolator.xy_list_interpolated:
            if self.stop_event.is_set():
                return
            self.printer.move_to_xy(x, y)
            self.drawn_points.put((x, y))

    def burst_printing(self):
        burst_size = self.printer.BURST_SIZE
        point_list = self.interpolator.xy_list_interpolated
        bursts = [point_list[i * burst_size: (i+1) * burst_size]
                  for i in range((len(point_list) - 1) // burst_size + 1)]
        for burst in bursts:
            if self.stop_event.is_set():
                return
            self.printer.burst(burst)
            for x, y in burst:
                self.drawn_points.put((x, y))