import tkinter
import tkinter as tk
from tkinter import ttk
import time
import typing
import serial
import math

from printer_commander import *
from drawing_process import *

# todo: refactor logging


def linear_map_to(val,
                  source_domain_lower, source_domain_upper,
                  target_domain_lower, target_domain_upper):
    return float(val - source_domain_lower) / (source_domain_upper - source_domain_lower) * \
           (target_domain_upper - target_domain_lower) + target_domain_lower


class App(tk.Tk):
    def __init__(self, canvas_width=650, canvas_height=650):
        super().__init__()

        self.canvas_width = canvas_width
        self.canvas_height = canvas_height

        self.title('Plotter')
        self.geometry(f"{canvas_width + 20}x{canvas_height + 100}+500+0")
        self.resizable(0, 0)

        self.previous_serial_command = ""
        self.current_serial_command = tkinter.StringVar()

        self.curr_xy = (0, 0)
        self.printer = PrinterCommander()

        self.filename = tkinter.StringVar(value="../gcode/test.gcode")
        self.drawing_process = DrawingProcess(self.printer, self.filename.get(), 1)

        self.create_body_frame()
        self.create_command_frame()

        self.protocol("WM_DELETE_WINDOW", self.on_closing)

    def on_closing(self):
        if self.drawing_process.is_alive():
            self.drawing_process.stop()
            self.drawing_process.join()
        self.printer.save_angles()
        self.destroy()

    def start_drawing(self):
        self.drawing_process = DrawingProcess(self.printer, self.filename.get(), interpolation_resolution=0.1, speed=200)
        self.drawing_process.start()
        self.monitor_drawing_process()

    def cancel_drawing(self):
        self.drawing_process.stop()
        pass

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
        self.current_serial_command.set("")
        self.serial_command_entry.insert(0, self.previous_serial_command)

    def send_serial_command(self, event):
        self.previous_serial_command = self.current_serial_command.get()
        self.printer.send_serial_command(self.current_serial_command.get())
        self.current_serial_command.set("")
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
        self.canvas = tk.Canvas(self,
                                width=self.canvas_width,
                                height=self.canvas_height, borderwidth=0, highlightthickness=0)
        self.canvas.create_rectangle(0, 0, self.canvas_width - 1, self.canvas_height - 1)

        self.canvas.bind('<ButtonPress-1>', self.canvas_click)
        # self.canvas.bind('<Button1-Motion>', self.canvas_click)
        # ^todo: BUG: button release not detected while printer head moving -> phantom move events. solution: separate thread for printer

        self.canvas.grid(column=0, row=0, sticky=tk.NSEW, padx=10, pady=10)

    def create_command_frame(self):
        self.controls_frame = ttk.Frame(self)
        self.controls_frame.grid(column=0, row=1, sticky=tk.EW)

        self.drawing_controls_frame = ttk.LabelFrame(self.controls_frame, text="Drawing process")
        self.drawing_controls_frame.grid(column=0, row=0, sticky=tk.W, padx=10, pady=0)

        self.file_name_entry = ttk.Entry(self.drawing_controls_frame,
                                              textvariable=self.filename,
                                              width=50)
        self.file_name_entry.pack(fill=tk.X)

        self.start_button = ttk.Button(self.drawing_controls_frame, text='Start')
        self.start_button['command'] = self.start_drawing
        self.start_button.pack(fill=tk.BOTH, side=tk.RIGHT)

        self.cancel_button = ttk.Button(self.drawing_controls_frame, text='Cancel')
        self.cancel_button['command'] = self.cancel_drawing
        self.cancel_button.pack(fill=tk.BOTH, side=tk.RIGHT)

        self.printer_controls_frame = ttk.LabelFrame(self.controls_frame, text="Printer Controls")
        self.printer_controls_frame.grid(column=1, row=0, sticky=tk.NW, padx=10, pady=0)


        self.serial_command_entry = ttk.Entry(self.printer_controls_frame,
                                              textvariable=self.current_serial_command,
                                              width=50)
        self.serial_command_entry.pack(fill=tk.X)
        self.serial_command_entry.bind('<Return>', self.send_serial_command, add='+')
        self.serial_command_entry.bind('<Up>', self.recall_previous_serial_command)

        self.autocalibrate_button = ttk.Button(self.printer_controls_frame, text='Auto Calibrate')
        self.autocalibrate_button['command'] = self.printer.autocalibrate
        self.autocalibrate_button.pack(fill=tk.BOTH, side=tk.RIGHT)

        self.reset_head_button = ttk.Button(self.printer_controls_frame, text='Reset Head')
        self.reset_head_button['command'] = self.reset_head
        self.reset_head_button.pack(fill=tk.BOTH, side=tk.RIGHT)

        self.zero_button = ttk.Button(self.printer_controls_frame, text='Zero Angles')
        self.zero_button['command'] = self.printer.zero_position
        self.zero_button.pack(fill=tk.BOTH, side=tk.RIGHT)

    def monitor_drawing_process(self):
        if self.drawing_process.is_alive():
            # check the thread every 100ms
            while not self.drawing_process.drawn_points.empty():
                point = self.drawing_process.drawn_points.get()
                self.canvas.create_line(*self.curr_xy, *self.screen_xy(*point))
                # print("line ", self.curr_xy, self.screen_xy(*point))
                self.curr_xy = self.screen_xy(*point)
            self.after(100, lambda: self.monitor_drawing_process())


if __name__ == "__main__":
    app = App()
    app.mainloop()