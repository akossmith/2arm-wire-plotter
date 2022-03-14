import tkinter
import tkinter as tk
from tkinter import ttk
from threading import Thread, Event
import time
import serial
from queue import Queue
import math
import typing
from gcodehandler import *

def linear_map_to(val,
                  source_domain_lower, source_domain_upper,
                  target_domain_lower, target_domain_upper):
    return float(val - source_domain_lower) / (source_domain_upper - source_domain_lower) * \
           (target_domain_upper - target_domain_lower) + target_domain_lower


class PrinterCommander:
    def __init__(self):
        # printer physical parameters:
        self.R1 = 115  # left arm length
        self.R2 = 115
        self.l1 = 155  # left wire length
        self.l2 = 155
        self.D = 272  # distance of motor axles

        # working area
        self.width = 80
        self.height = 80
        self.x_min = (self.D - self.width) / 2.0
        self.y_min = -20

        self.curr_alpha1 = 0.0
        self.curr_alpha2 = 0.0

        self.serial = serial.Serial('COM5', 115200, timeout=1000, parity=serial.PARITY_NONE)
        text = self.serial.readline()
        print(text)
        pass

    @property
    def workspace_width(self):
        return self.width

    @property
    def workspace_height(self):
        return self.height

    def getAlphas(self, x: float, y: float) -> typing.Tuple[float, float]:
        """x,y in printer coordinates, return: degrees"""


        # printer physical parameters:
        R1 = self.R1
        R2 = self.R2
        l1 = self.l1
        l2 = self.l2
        D = self.D

        #
        x = x + self.x_min
        y = y + self.y_min
        print(f"x,y: {x:3.5f} {y:3.5f}", end=' ')

        # formulae valid for angles in [0, 180deg] (practically meaningful: [0, ~100deg])
        ca1 = (x * (R1 ** 2 - l1 ** 2 + x ** 2 + y ** 2) + y * math.sqrt(
            (-R1 ** 2 + 2 * R1 * l1 - l1 ** 2 + x ** 2 + y ** 2) * (
                    R1 ** 2 + 2 * R1 * l1 + l1 ** 2 - x ** 2 - y ** 2))) / (2 * R1 * (x ** 2 + y ** 2))
        sa1 = math.sqrt(1 - ca1 ** 2)

        ca2 = (y * math.sqrt((-D ** 2 + 2 * D * x + R2 ** 2 + 2 * R2 * l2 + l2 ** 2 - x ** 2 - y ** 2) * (
                D ** 2 - 2 * D * x - R2 ** 2 + 2 * R2 * l2 - l2 ** 2 + x ** 2 + y ** 2)) + (D - x) * (
                       D ** 2 - 2 * D * x + R2 ** 2 - l2 ** 2 + x ** 2 + y ** 2)) / (
                      2 * R2 * (D ** 2 - 2 * D * x + x ** 2 + y ** 2))
        sa2 = math.sqrt(1 - ca2 ** 2)

        alpha1 = math.atan2(sa1, ca1) / math.pi * 180
        alpha2 = math.atan2(sa2, ca2) / math.pi * 180  # by our convention positive is up (as opposed to math)
        print(f"alpha1: {alpha1:.5f} alpha2: {alpha2:.5f}")
        return alpha1, alpha2

    def move_to_alphas(self, alpha1_deg: float, alpha2_deg: float):
        # alpha1, alpha2 = alphas_deg
        print(f'requested\t l{alpha1_deg - self.curr_alpha1}r{alpha2_deg - self.curr_alpha2}')

        self.serial.write(f'l{alpha1_deg - self.curr_alpha1}r{alpha2_deg - self.curr_alpha2}'.encode('ascii'))
        text = self.serial.readline().decode("ascii")
        print(text)
        left_str, right_str = text.split()
        actual_left_angle_delta = float(left_str[2:])
        actual_right_angle_delta = float(right_str[2:])

        print(f'actual\t\t l{actual_left_angle_delta}r{actual_right_angle_delta}')
        self.curr_alpha1 = self.curr_alpha1 + actual_left_angle_delta
        self.curr_alpha2 = self.curr_alpha2 + actual_right_angle_delta

    def move_to_xy(self, x: float, y: float):
        alphas = self.getAlphas(x, y)
        self.move_to_alphas(*alphas)

    def reset_head(self):
        self.move_to_alphas(0, 0)

    def set_rpm(self, rpm: float):
        self.serial.write(f's{rpm}'.encode('ascii'))
        text = self.serial.readline().decode("ascii") # need to empty buffer
        print(text)

    def send_serial_command(self, command: str):
        self.serial.write(command.encode('ascii'))
        text = self.serial.readline().decode("ascii")
        print(text)

class DrawingProcess(Thread):
    def __init__(self, printer: PrinterCommander, stop_event: Event):
        super().__init__()
        self.stop_event = stop_event
        self.printer = printer
        self.drawn_points = Queue()

    def run(self):
        self.printer.set_rpm(300)
        interpolator = GCodeInterpolator(read_gcode_file("../kor.ngc"), max_point_distance_mm=1)
        for point in interpolator.xy_list_raw:
            if self.stop_event.is_set():
                return
            x = point[0]
            y = point[1]
            self.printer.move_to_xy(x, y)
            self.drawn_points.put((x, y))


class App(tk.Tk):
    def __init__(self, canvas_width=500, canvas_height=500):
        super().__init__()

        self.canvas_width = canvas_width
        self.canvas_height = canvas_height

        self.title('Webpage Download')
        self.geometry('600x600')
        self.resizable(0, 0)

        self.previous_serial_command = ""
        self.current_serial_command = tkinter.StringVar()

        self.create_body_frame()
        self.create_command_frame()

        self.printer = PrinterCommander()

        self.stop_event = Event()
        self.drawing_process = DrawingProcess(self.printer, self.stop_event)
        self.drawing_process.start()
        self.curr_xy = (0, 0)
        self.monitor_drawing_process()

    def target_xy(self, screen_x, screen_y):
        return (
            linear_map_to(screen_x, 0, self.canvas_width, 0, self.printer.workspace_width),
            linear_map_to(screen_y, 0, self.canvas_height, 0, self.printer.workspace_height)
        )

    def screen_xy(self, printer_x, printer_y):
        return (
            linear_map_to(printer_x, 0, self.printer.workspace_width, 0, self.canvas_width),
            linear_map_to(printer_y, 0, self.printer.workspace_width, 0, self.canvas_height)
        )

    def put_marker(self, x, y, **kwargs):
        r = 2
        return self.canvas.create_oval(x - r, y - r, x + r, y + r, **kwargs)

    def recall_previous_serial_command(self, event):
        self.current_serial_command.set(self.previous_serial_command)

    def send_serial_command(self, event):
        self.previous_serial_command = self.current_serial_command.get()
        self.printer.send_serial_command(self.current_serial_command.get())
        pass

    def cancel_drawing(self):
        self.stop_event.set()
        pass

    def reset_head(self):
        if self.drawing_process.is_alive():
            return
        self.printer.move_to_alphas(0.0, 0.0)
        pass

    def canvas_click(self, event):
        if self.drawing_process.is_alive():
            return
        self.put_marker(event.x, event.y)
        # self.canvas.create_line(self.curr_xy, event.x, event.y)
        self.printer.move_to_xy(*self.target_xy(event.x, event.y))

    def create_body_frame(self):
        self.drawing_area_frame = ttk.Frame(self)
        self.canvas = tk.Canvas(self.drawing_area_frame,
                                width=self.canvas_width,
                                height=self.canvas_height, borderwidth=0, highlightthickness=0)
        self.canvas.create_rectangle(0, 0, self.canvas_width - 1, self.canvas_height - 1)

        self.canvas.bind('<ButtonPress-1>', self.canvas_click)
        # self.canvas.bind('<Button1-Motion>', self.canvas_click)
        # ^todo: BUG: button release not detected while printer head moving -> phantom move events. solution: separate thread for printer

        self.canvas.grid(column=0, row=0, sticky=tk.NSEW, padx=10, pady=10)

        self.drawing_area_frame.grid(column=0, row=0, sticky=tk.NSEW, padx=10, pady=10)

    def create_command_frame(self):
        self.command_frame = ttk.Frame(self)

        self.command_frame.columnconfigure(0, weight=5)
        self.command_frame.columnconfigure(1, weight=2)
        self.command_frame.columnconfigure(2, weight=2)

        self.serial_command_entry = ttk.Entry(self.command_frame,
                                              textvariable=self.current_serial_command,
                                              width=40)
        self.serial_command_entry.grid(column=0, row=0, sticky=tk.E)
        self.serial_command_entry.bind('<Return>', self.send_serial_command, add='+')
        self.serial_command_entry.bind('<Up>', self.recall_previous_serial_command)

        self.cancel_button = ttk.Button(self.command_frame, text='Cancel Drawing Process')
        self.cancel_button['command'] = self.cancel_drawing
        self.cancel_button.grid(column=1, row=0, sticky=tk.E)

        self.reset_button = ttk.Button(self.command_frame, text='Reset Head')
        self.reset_button['command'] = self.reset_head
        self.reset_button.grid(column=2, row=0, sticky=tk.E)

        self.command_frame.grid(column=0, row=1, sticky=tk.NSEW, padx=10, pady=10)

    def monitor_drawing_process(self):
        if self.drawing_process.is_alive():
            # check the thread every 100ms
            while not self.drawing_process.drawn_points.empty():
                point = self.drawing_process.drawn_points.get()
                self.canvas.create_line(*self.curr_xy, *self.screen_xy(*point))
                print("line ", self.curr_xy, self.screen_xy(*point))
                self.curr_xy = self.screen_xy(*point)
            self.after(100, lambda: self.monitor_drawing_process())


if __name__ == "__main__":
    app = App()
    app.mainloop()