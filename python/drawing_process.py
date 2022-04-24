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
        self.printing_method = self.regular_printing

    def stop(self):
        self.stop_event.set()

    def run(self):
        self.printer.set_rpm(self.speed)
        self.printing_method()

    def regular_printing(self):
        previous_z = 1
        for x, y, z in self.interpolator.xy_list_interpolated:
            if self.stop_event.is_set():
                return
            if z < 0 <= previous_z:
                self.printer.pen_down()
            elif z > 0 >= previous_z:
                self.printer.pen_up()
            previous_z = z
            self.printer.move_to_xy(x, y)
            self.drawn_points.put((x, y))

    def burst_printing(self):
        burst_size = self.printer.BURST_SIZE
        previous_z = 1
        curr_burst: typing.List[typing.Tuple[float, float]] = []  # z coordinate fixed within burst
        for p in self.interpolator.xy_list_interpolated:
            if self.stop_event.is_set():
                return
            if math.copysign(1, p[2]) == math.copysign(1, previous_z) \
                    and len(curr_burst) < burst_size:
                curr_burst.append(p[0:2])
            else:
                self.printer.burst(curr_burst)
                if math.copysign(1, p[2]) != math.copysign(1, previous_z):
                    if math.copysign(1, p[2]) < 0:
                        self.printer.pen_down()
                    else:
                        self.printer.pen_up()
                    previous_z = p[2]
                curr_burst = []