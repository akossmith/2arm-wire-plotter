import math

R1 = 115  # left arm length
R2 = 113
l1 = 161  # left wire length
l2 = 160
D = 272  # distance of motor axles

square_width = 80
x_range_margin = (D - square_width) / 2.0

target_x_range = (x_range_margin, D - x_range_margin)
target_y_range = (-20, 60)


def getAlphas(x, y):
    print(f"x,y: {x:3.5f} {y:3.5f}", end=' ')

    # formulae valid for angles in [0, 180deg] (practically meaningful: [0, ~100deg])
    ca1 = (x * (R1 ** 2 - l1 ** 2 + x ** 2 + y ** 2) + y * math.sqrt(
        (-R1 ** 2 + 2 * R1 * l1 - l1 ** 2 + x ** 2 + y ** 2) * (R1 ** 2 + 2 * R1 * l1 + l1 ** 2 - x ** 2 - y ** 2))) / (
                      2 * R1 * (x ** 2 + y ** 2))
    sa1 = math.sqrt(1 - ca1 ** 2)

    ca2 = (y * math.sqrt((-D ** 2 + 2 * D * x + R2 ** 2 + 2 * R2 * l2 + l2 ** 2 - x ** 2 - y ** 2) * (
                D ** 2 - 2 * D * x - R2 ** 2 + 2 * R2 * l2 - l2 ** 2 + x ** 2 + y ** 2)) + (D - x) * (
                       D ** 2 - 2 * D * x + R2 ** 2 - l2 ** 2 + x ** 2 + y ** 2)) / (
                      2 * R2 * (D ** 2 - 2 * D * x + x ** 2 + y ** 2))
    sa2 = math.sqrt(1 - ca2 ** 2)

    alpha1 = math.atan2(sa1, ca1) / math.pi * 180
    alpha2 = math.atan2(sa2, ca2) / math.pi * 180  # by convenion positive is up (as opposed to math)
    print(f"alpha1: {alpha1:.5f} alpha2: {alpha2:.5f}")
    return alpha1, alpha2


def linear_map_to(val,
                  source_domain_lower, source_domain_upper,
                  target_domain_lower, target_domain_upper):
    return float(val - source_domain_lower) / (source_domain_upper - source_domain_lower) * \
           (target_domain_upper - target_domain_lower) + target_domain_lower


max_point_dist = 2
curr_x = 0.0
curr_y = 0.0

refined_point_list = [(0, 0)]

with open("../kor.ngc") as f:
    lines = f.readlines()
    print(lines)

    for line in lines[:]:
        if line.startswith("G00 X"):
            splitted = line.split()
            curr_x = float(splitted[1][1:])
            curr_y = float(splitted[2][1:])

            print(f"({curr_x},{curr_y}),", end='')
            refined_point_list.append((curr_x, curr_y))
            # xs.append(curr_x)
            # ys.append(curr_y)

        if line.startswith("G02 X"):
            splitted = line.split()
            x = float(splitted[1][1:])
            y = float(splitted[2][1:])
            i = float(splitted[4][1:])
            j = float(splitted[5][1:])

            R = math.sqrt(i ** 2 + j ** 2)
            cos_phi = (-i * (x - curr_x - i) - j * (y - curr_y - j)) / (i ** 2 + j ** 2)
            sgn_phi = -1 if (i * (y - curr_y) - j * (x - curr_x)) > 0 else 1
            phi = sgn_phi * math.acos(cos_phi)  # + 2*math.pi, 2*math.pi)
            if phi > 0:
                phi -= 2 * math.pi  # always need a negative value, since going CW
            gamma = math.atan2(-j, -i)

            max_angle_dist = max_point_dist / R

            num_new_points = int(phi / max_angle_dist)
            curr_angle = gamma

            curr_angle -= max_angle_dist
            while curr_angle > phi + gamma:
                new_x = curr_x + i + math.cos(curr_angle) * R
                new_y = curr_y + j + math.sin(curr_angle) * R
                refined_point_list.append((new_x, new_y))
                # xs.append(new_x)
                # ys.append(new_y)
                print(f"({new_x},{new_y}),", end='')
                curr_angle -= max_angle_dist
            # for ind in range(1, num_new_points):
            #     angle = gamma + ind * max_angle_dist
            #     new_x = curr_x + i + math.cos(angle)*R
            #     new_y = curr_y + j + math.sin(angle)*R
            #     refined_point_list.append((new_x, new_y))
            #     print(f"({new_x},{new_y}),", end='')

            # x = float(splitted[1][1:]) + target_x_range[0]
            # y = float(splitted[1][1:]) + target_y_range[0]
            print(f"({x},{y}),", end='')
            refined_point_list.append((x, y))
            curr_x = x
            curr_y = y
            # alphas = getAlphas(x, y)
            # move_to_alphas(alphas)

from matplotlib import pyplot as plt

xs, ys = list(zip(*tuple(refined_point_list)))
plt.plot(xs, ys)
plt.show()
