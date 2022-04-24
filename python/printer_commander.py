import typing
import serial
import math


class PrinterCommander:
    BURST_SIZE = 15  # each point is 2 bytes -> 15*2*2+4(checksum) = 64 = Arduino serial buffer size

    def __init__(self):
        # printer physical parameters (distances in mm):
        # lego arms attached to metal wheel
        self.R1 = 99.625 - 3 * 7.97  # 99.625 <- last hole (one hole distance is 7.97mm)
        self.R2 = 99.625 - 3 * 7.97
        self.l1 = 159  # left wire length
        self.l2 = 159
        self.D = 258.7  # distance of motor axles

        # working area
        self.width = 80
        self.height = 80
        self.x_min = (self.D - self.width) / 2.0
        self.y_min = 15

        self.curr_alpha1 = 0.0
        self.curr_alpha2 = 0.0

        self.serial = serial.Serial('COM5', 115200, timeout=1000, parity=serial.PARITY_NONE)
        startup_response = self.serial.readline().decode("ascii")
        print(startup_response)

        response = self.send_serial_command("getcurrangles")
        self.__parse_anlges_response(response)
        pass

    @property
    def workspace_width(self):
        return self.width

    @property
    def workspace_height(self):
        return self.height

    @property
    def current_alphas(self) -> typing.Tuple[float, float]:
        return self.curr_alpha1, self.curr_alpha2

    def pen_up(self):
        self.send_serial_command("penup")

    def pen_down(self):
        self.send_serial_command("pendown")

    def move_to_alphas(self, alpha1_deg: float, alpha2_deg: float):
        print(f'requested\t l{alpha1_deg}r{alpha2_deg}')

        response = self.send_serial_command(f'moveto l{alpha1_deg} r{alpha2_deg}')
        self.__parse_anlges_response(response)

    def move_to_xy(self, x: float, y: float):
        alphas = self.__getAlphas(x, y)
        self.move_to_alphas(*alphas)

    def reset_head(self):
        self.move_to_alphas(0, 0)

    def calibrate(self, alpha1_deg: float, alpha2_deg: float):
        self.send_serial_command(f"calibrate l{alpha1_deg} r{alpha2_deg}")
        self.curr_alpha1 = alpha1_deg
        self.curr_alpha2 = alpha2_deg

    def autocalibrate(self):
        response = self.send_serial_command("autocal")
        self.__parse_anlges_response(response)

    def set_rpm(self, rpm: float):
        self.send_serial_command(f"setspeed {rpm}")

    def zero_position(self):
        self.send_serial_command(f'zeroangles')

    def save_angles(self):
        self.send_serial_command(f'saveangles')

    def send_serial_command(self, command: str, terminator="\n") -> str:
        self.serial.write((command + terminator).encode('ascii'))
        response = self.serial.readline().decode("ascii")
        print("Plotter response: ", response)
        return response

    def burst(self, xys: typing.Collection[typing.Tuple[float, float]]):
        assert len(xys) <= self.__class__.BURST_SIZE

        alphass = [self.__getAlphas(x, y) for x, y in xys]  # has to be evaluated eagerly, so as the get exception here

        def serializedNumber(number: float) -> bytes:
            fraction = int(number % 1 * 255 + .5)
            return bytes([int(number), fraction])

        def serializedPointList(point_list: typing.Collection[typing.Tuple[float, float]]):
            res = bytearray()
            checksum = 0
            for a, b in point_list:
                res += serializedNumber(a)
                res += serializedNumber(b)
                checksum = (checksum + int.from_bytes(serializedNumber(a), byteorder='big') * 0x10000 +
                            int.from_bytes(serializedNumber(b), byteorder='big')) % 0x100000000
            return res + checksum.to_bytes(4, byteorder="big", signed=False)

        self.send_serial_command(f"burst s{len(xys)}")

        self.serial.write(serializedPointList(alphass))
        text = self.serial.readline().decode("ascii")
        while not text.startswith("ok "):
            print("Plotter response: ", text)
            self.serial.write(serializedPointList(alphass))
            text = self.serial.readline().decode("ascii")

        print("Plotter response: ", text)
        self.__parse_anlges_response(text)
        print(f'actual\t\t l{self.curr_alpha1}r{self.curr_alpha2}')

    def __getAlphas(self, x: float, y: float) -> typing.Tuple[float, float]:
        """x,y in printer coordinates, return: degrees"""

        # printer physical parameters:
        R1 = self.R1
        R2 = self.R2
        l1 = self.l1
        l2 = self.l2
        D = self.D

        # x = -x + self.x_min + self.width # mirroring
        # y = self.height - y + self.y_min  # vertical mirroring
        # x = x * 0.8  # scaling
        # y = y * 0.8

        x = x + self.x_min
        y = y + self.y_min
        print(f"x,y: {x:3.5f} {y:3.5f}", end=' ')

        # formulae valid for angles in [0, 180deg] (practically meaningful: [0, ~130deg])
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

    def __parse_anlges_response(self, text):
        self.curr_alpha1, self.curr_alpha2 = map(float, text.split()[1:])