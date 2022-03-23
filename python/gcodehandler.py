import math
import re
import typing
import itertools
from collections import namedtuple


def read_gcode_file(filename: str) -> typing.Iterable[str]:
    with open(filename) as f:
        lines = f.readlines()
    return filter(lambda x: x[0].isalpha(), lines)


T = typing.TypeVar('T')


def remove_duplicates(iterable: typing.Iterable[T]) -> typing.Iterable[T]:
    return (key for key, _ in itertools.groupby(iterable))

Command = namedtuple('Command', ['letter', 'number'])

class GCodeInterpolator:
    def __init__(self,
                 gcode_instruction_list: typing.Iterable[str],
                 max_point_distance_mm: float = 1):

        only_command_lines = filter(lambda l: len(l) > 0 and l[0].upper() == "G", gcode_instruction_list)
        comments_stripped = map(lambda l: re.sub(r"\(.*?\)", "", l), only_command_lines)
        comments_stripped = map(lambda l: re.sub(r";.*?$", "", l), comments_stripped)
        upper_cased = map(lambda l: l.upper(), comments_stripped)
        self.gcode_instruction_lines = list(upper_cased)

        self.max_point_dist = max_point_distance_mm

    @staticmethod
    def coords(line):
        # assert line[0].upper() == "G0"

        words = line.split()
        try:
            return {word[0].upper(): float(word[1:]) for word in words[1:]}
        except Exception:
            print(words)
            return {} #todo: fix this

    @staticmethod
    def command(line) -> Command: # typing.Tuple[str, int]:
        # todo: change representation: (letter, number)
        comm = line.split()[0]
        return Command(comm[0].upper(), int(comm[1:]))

    # todo: filter for z > 0 ?

    @property
    def raw_coord_list(self) -> typing.Iterable[dict[str, float]]:
        curr_point = {}
        point_list = []
        for line in self.gcode_instruction_lines:
            command = GCodeInterpolator.command(line)
            if command.letter == "G" and command.number in [0, 1, 2, 3]:
                coords = GCodeInterpolator.coords(line)
                curr_point = {**curr_point, **coords}  # merging
                # print(f"({curr_x},{curr_y}),", end='')
                point_list.append(curr_point)

        point_list = itertools.dropwhile(lambda d: "X" not in d or "Y" not in d, point_list)  # potential initial z axis adjustment
        return point_list

    @property
    def xy_list_raw(self) -> typing.Collection[typing.Tuple[float, float]]:
        res = remove_duplicates(map(lambda coords: (coords['X'], coords['Y']),
                                  self.raw_coord_list))
        return list(res)

    @property
    def xy_list_interpolated(self) -> typing.Collection[typing.Tuple[float, float]]:
        curr_point = {}
        point_list = []
        for line in self.gcode_instruction_lines:
            command = GCodeInterpolator.command(line)
            coords = GCodeInterpolator.coords(line)

            if "X" not in coords and "Y" not in coords:  # no x,y change, and we have no z axis
                continue

            if command.letter == "G" and \
                    (command.number == 0
                     or any(coord not in curr_point for coord in ["X", "Y"])):  # no meaningful current point yet
                curr_point = {**curr_point, **coords}
                point_list.append((curr_point['X'], curr_point['Y']))
            elif command.letter == "G" and (command.number in [1]):
                x = coords["X"]
                y = coords["Y"]
                curr_x = curr_point['X']
                curr_y = curr_point['Y']

                dvx = x - curr_x
                dvy = y - curr_y
                dv_norm = math.sqrt(dvx**2 + dvy**2)

                if dv_norm > 0.00000001:  # just Z coord change
                    dvx_normed = dvx / dv_norm
                    dvy_normed = dvy / dv_norm

                    curr_len = 0.0
                    while curr_len < dv_norm:
                        new_x = curr_x + dvx_normed * curr_len
                        new_y = curr_y + dvy_normed * curr_len
                        curr_len += self.max_point_dist
                        point_list.append((new_x, new_y))
                point_list.append((x, y))

                curr_point = {**curr_point, **coords}
                # print(f"({curr_x},{curr_y}),", end='')
                # point_list.append((curr_point['X'], curr_point['Y']))

            elif command.letter == "G" and (command.number in [2, 3]):
                cw_dir = command.number == 2

                x = coords["X"]
                y = coords["Y"]
                i = coords["I"]
                j = coords["J"]
                curr_x = curr_point['X']
                curr_y = curr_point['Y']

                R = math.sqrt(i ** 2 + j ** 2)
                cos_phi = (-i * (x - curr_x - i) - j * (y - curr_y - j)) / (i ** 2 + j ** 2)
                sgn_phi = -1 if (i * (y - curr_y) - j * (x - curr_x)) > 0 else 1
                cos_phi = max(-1, min(1, cos_phi))  # can be invalid due roundoff errors
                phi = sgn_phi * math.acos(cos_phi)

                if cw_dir:
                    if phi > 0:
                        phi -= 2 * math.pi  # always need a negative value, since going CW
                else:
                    if phi < 0:
                        phi += 2 * math.pi  # always need a positive value, since going CCW
                gamma = math.atan2(-j, -i)

                max_angle_dist = self.max_point_dist / R

                curr_angle = gamma

                increment_sgn = -1 if cw_dir else 1
                curr_angle += max_angle_dist * increment_sgn
                while (cw_dir and curr_angle > phi + gamma) or \
                        (not cw_dir and curr_angle < phi + gamma):
                    new_x = curr_x + i + math.cos(curr_angle) * R
                    new_y = curr_y + j + math.sin(curr_angle) * R
                    point_list.append((new_x, new_y))
                    # print(f"({new_x},{new_y}),", end='')
                    curr_angle += max_angle_dist * increment_sgn
                # print(f"({x},{y}),", end='')
                point_list.append((x, y))
                curr_point = {**curr_point, **coords}

        return list(remove_duplicates(point_list))

    @property
    def max_point_distcane_mm(self):
        return self.max_point_dist

    @max_point_distcane_mm.setter
    def max_point_distcane_mm(self, max_point_distcane_mm):
        self.max_point_distcane_mm = max_point_distcane_mm