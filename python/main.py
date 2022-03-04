import math
import tkinter as tk
import serial


def _create_circle(self, x, y, r, **kwargs):
    return self.create_oval(x - r, y - r, x + r, y + r, **kwargs)
tk.Canvas.create_circle = _create_circle

ser = serial.Serial('COM5', 115200, timeout=1000, parity=serial.PARITY_NONE)
text = ser.readline()
print(text)

# s = ser.write('l-50r-10'.encode('utf-8'))
# s = ser.write('l0r10'.encode('ascii'))
# while True:
#     while ser.in_waiting > 0:
#         text = ser.readline().decode('utf-8')
#         print(text)

# ser.close()

root = tk.Tk()

canvas_width = 500
canvas_height = 500

curr_alpha1 = 0.0
curr_alpha2 = 0.0

R1 = 115 #left arm length
R2 = 113
l1 = 161 #left wire length
l2 = 160
D = 272 #distance of motor axles

square_width = 80
y_range_margin = (D - square_width) / 2.0

target_x_range = (y_range_margin, D - y_range_margin)
target_y_range = (-35, 25)


def getAlphas(x, y):
    print(f"x,y: {x:3.5f} {y:3.5f}", end=' ')

    # formulae valid for angles in [0, 180deg] (practically meaningful: [0, ~100deg])
    ca1 = (x*(R1**2 - l1**2 + x**2 + y**2) + y*math.sqrt((-R1**2 + 2*R1*l1 - l1**2 + x**2 + y**2)*(R1**2 + 2*R1*l1 + l1**2 - x**2 - y**2)))/(2*R1*(x**2 + y**2))
    sa1 = math.sqrt(1 - ca1**2)

    ca2 = (y*math.sqrt((-D**2 + 2*D*x + R2**2 + 2*R2*l2 + l2**2 - x**2 - y**2)*(D**2 - 2*D*x - R2**2 + 2*R2*l2 - l2**2 + x**2 + y**2)) + (D - x)*(D**2 - 2*D*x + R2**2 - l2**2 + x**2 + y**2))/(2*R2*(D**2 - 2*D*x + x**2 + y**2))
    sa2 = math.sqrt(1 - ca2 ** 2)

    alpha1 = math.atan2(sa1, ca1) / math.pi * 180
    alpha2 = math.atan2(sa2, ca2) / math.pi * 180 # by convenion positive is up (as opposed to math)
    print(f"alpha1: {alpha1:.5f} alpha2: {alpha2:.5f}")
    return alpha1, alpha2


def linear_map_to(val,
                  source_domain_lower, source_domain_upper,
                  target_domain_lower, target_domain_upper):
    return float(val - source_domain_lower) / (source_domain_upper - source_domain_lower) * \
           (target_domain_upper - target_domain_lower) + target_domain_lower


def target_x(x):
    return linear_map_to(x, 0, canvas_width, target_x_range[0], target_x_range[1])


def target_y(y):
    return linear_map_to(y, 0, canvas_height, target_y_range[0], target_y_range[1])


def move_to_alphas(alphas):
    global curr_alpha1
    global curr_alpha2
    alpha1, alpha2 = alphas
    print(f'requested\t l{alpha1 - curr_alpha1}r{alpha2 - curr_alpha2}')

    ser.write(f'l{alpha1 - curr_alpha1}r{alpha2 - curr_alpha2}'.encode('ascii'))

    text = ser.readline().decode("ascii")
    actual_left_angle_delta = float(text[2:])
    text = ser.readline().decode("ascii")
    actual_right_angle_delta = float(text[2:])
    print(f'actual\t l{actual_left_angle_delta}r{actual_right_angle_delta}')
    curr_alpha1 = curr_alpha1 + actual_left_angle_delta
    curr_alpha2 = curr_alpha2 + actual_right_angle_delta


button_is_pressed = False


def put_circle(x, y):
    w.create_circle(x, y, 3)
    alphas = getAlphas(target_x(x), target_y(y))
    move_to_alphas(alphas)


def button_down(event):
    global button_is_pressed
    button_is_pressed = True
    put_circle(event.x, event.y)


def button_up(event):
    global button_is_pressed
    button_is_pressed = False


def mmove(event):
    put_circle(event.x, event.y)


def on_closing():
    move_to_alphas((0, 0))
    #time.sleep(3)
    ser.close()
    root.destroy()


root.protocol("WM_DELETE_WINDOW", on_closing)

root.bind('<ButtonRelease-1>', button_up)
root.bind('<Button-1>', button_down)
root.bind('<Button1-Motion>', mmove)

w = tk.Canvas(root,
              width=canvas_width,
              height=canvas_height, borderwidth=0, highlightthickness=0)
w.pack()

y = int(canvas_height / 2)
w.create_rectangle(0, 0, canvas_width-1, canvas_height-1)
root.mainloop()

# Press the green button in the gutter to run the script.
if __name__ == '__main__':
    print('hello')
