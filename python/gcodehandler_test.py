import unittest

from gcodehandler import *


class TestCaseWithAllAlmostEqual(unittest.TestCase):
    def assertAllAlmostEquals(self,
                              expected: typing.Sequence[typing.Sequence[float]],
                              actual: typing.Sequence[typing.Sequence[float]],
                              *args,
                              **kwargs ):
        def flatten(arr):
            return (elem for pair in arr for elem in pair)

        self.assertEqual(len(expected), len(actual), "Number of points doesn't match")
        for pair in zip(expected, actual):
            self.assertEqual(len(pair[0]), len(pair[1]), "Number of coordinates doesn't match")

        for pair in zip(flatten(expected), flatten(actual)):
            self.assertAlmostEqual(pair[0], pair[1], *args, **kwargs)


class GCodeInterpolatorBasicTest(TestCaseWithAllAlmostEqual):
    def setUp(self):
        text = [
            "(Header)\n",
            "M3\n",
            "(Header end.)\n",
            "G21 (All units in mm)\n",
            "(Change tool to Default tool)\n",
            "\n",
            "g00 Z5.000000\n",
            "G00 X32.758016 Y69.299180\n",
            "\n",
            "G01 Z-0.125000 F100.0(Penetrate)\n",
            "g03 X29.636206 y68.702469 Z-0.125000 I4.877502 J-33.982124 F400.000000\n",
            "G03 X25.484210 y67.556340 Z-0.125000 I14.691402 J-61.315086"
        ]
        self.interp = GCodeInterpolator(text, 2)

    def test_no_comments(self):
        self.assertNotIn("(", ''.join(self.interp.gcode_instruction_lines))
        self.assertNotIn(")", ''.join(self.interp.gcode_instruction_lines))

    def test_uppercased(self):
        self.assertTrue(all(not ch.isalpha() or ch.isupper() for ch in ''.join(self.interp.gcode_instruction_lines)))

    def test_only_command_lines(self):
        for line in self.interp.gcode_instruction_lines:
            self.assertTrue(line[0].isalpha())

    def test_raw_xy(self):
        expected = [
            (32.758016, 69.299180),
            (29.636206, 68.702469),
            (25.484210, 67.556340)
        ]
        self.assertEqual(expected, self.interp.xy_list_raw)

    def test_interpolated_xy_regression_test(self):
        expected = [
            (32.758016, 69.29918),
            (30.787698575660663, 68.95753974270235),
            (29.636206, 68.702469),
            (27.698973927426756, 68.20568281833968),
            (25.778472081495725, 67.64770676771514),
            (25.48421, 67.55634)
        ]
        self.assertAllAlmostEquals(expected, self.interp.xy_list_interpolated, delta=0.01)


class GCodeInterpolatorInterpolationTest(TestCaseWithAllAlmostEqual):
    def test_g2_interpolation(self):
        text = [
            "G00 X1 Y1\n",
            "G02 X3 Y3 i2 j0\n",
        ]
        max_point_dist = 1
        interp = GCodeInterpolator(text, max_point_dist)
        print(interp.xy_list_interpolated)
        expected = [
            (1, 1),
            (1.2448348762192545, 1.958851077208406),
            (1.9193953882637205, 2.682941969615793),
            (2.8585255966645944, 2.994989973208109),
            (3, 3)
        ]
        self.assertAllAlmostEquals(expected, interp.xy_list_interpolated, delta=0.01)

    def test_g3_interpolation(self):
        text = [
            "G00 X3 Y3\n",
            "G03 X1 Y1 i0 j-2\n",
        ]
        max_point_dist = 1
        interp = GCodeInterpolator(text, max_point_dist)
        expected = [
            (3, 3),
            (2.041148922791594, 2.7551651237807455),
            (1.317058030384207, 2.0806046117362795),
            (1.005010026791891, 1.1414744033354058),
            (1, 1),
        ]
        self.assertAllAlmostEquals(expected, interp.xy_list_interpolated, delta=0.01)

    def test_linear_interpolation(self):
        text = [
            "G00 X3 Y3\n",
            "G01 X7.5 Y7.5\n",
        ]
        max_point_dist = math.sqrt(2)
        interp = GCodeInterpolator(text, max_point_dist)
        expected = [
            (3, 3),
            (4, 4),
            (5, 5),
            (6, 6),
            (7, 7),
            (7.5, 7.5),
        ]
        self.assertAllAlmostEquals(expected, interp.xy_list_interpolated, delta=0.01)

    def test_linear_interpolation2(self):
        text = [
            "G00 X0 Y0\n",
            "G01 X-2 Y-4\n",
        ]
        max_point_dist = math.sqrt(5)
        interp = GCodeInterpolator(text, max_point_dist)
        expected = [
            (0, 0),
            (-1, -2),
            (-2, -4),
        ]
        self.assertAllAlmostEquals(expected, interp.xy_list_interpolated, delta=0.01)


if __name__ == '__main__':
    unittest.main()
