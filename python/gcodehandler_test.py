import unittest

from gcodehandler import *


class GCodeInterpolatorBasicTest(unittest.TestCase):
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
        self.interp = GCodeInterpolator(text)
        print(self.interp.xy_list_raw)

    def test_no_comments(self):
        self.assertNotIn("(", ''.join(self.interp.gcode_instruction_lines))
        self.assertNotIn(")", ''.join(self.interp.gcode_instruction_lines))

    def test_uppercased(self):
        print(self.interp.gcode_instruction_lines)
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

# todo: interpolation test?

if __name__ == '__main__':
    unittest.main()