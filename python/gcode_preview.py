from gcodehandler import *

# interpolator = GCodeInterpolator(read_gcode_file("../kutya_0009.ngc"), max_point_distance_mm=1)
# refined_point_list = interpolator.xy_list_interpolated
#
interpolator = GCodeInterpolator(read_gcode_file("../text.ngc"), max_point_distance_mm=1)
refined_point_list = interpolator.xy_list_interpolated


# interpolator = GCodeInterpolator(
# """"
#
# G00 Z5.000000
# G00 X41.609375 Y44.632850
#
# G01 Z-0.125000 F100.0(Penetrate)
# G03 X32.818485 Y43.747992 Z-0.125000 I-4.356106 J-0.833258 F400.000000
# G03 X41.587895 Y43.748090 Z-0.125000 I4.384705 J0.051017
# G02 X41.929227 Y40.753557 Z-0.125000 I-49.341891 J-7.140951
# G02 X41.875004 Y39.429730 Z-0.125000 I-5.329085 J-0.444750
# G02 X41.431496 Y38.245726 Z-0.125000 I-3.246600 J0.541053
# G02 X40.435551 Y36.896520 Z-0.125000 I-8.489708 J5.224670
# G03 X38.352510 Y32.675513 Z-0.125000 I6.829541 J-5.994825
# G03 X38.783207 Y28.660200 Z-0.125000 I6.630222 J-1.319574
# G01 X39.037113 Y28.076210 Z-0.125000
# G01 X39.417973 Y28.927770 Z-0.125000
# G02 X39.642701 Y29.379814 Z-0.125000 I5.810660 J-2.606809
# """.split('\n'), 1)
# refined_point_list = interpolator.xy_list_interpolated

from matplotlib import pyplot as plt

xs, ys, zs = list(zip(*tuple(refined_point_list)))
plt.plot(xs, ys)
plt.show()

interpolator = GCodeInterpolator(
""""G00 X41.959031 Y44.330170 Z-0.125000 I-2.211116 J-0.987637
G03 X41.714921 Y44.596500 Z-0.125000 I-0.924568 J-0.602392
G03 X41.609422 Y44.632900 Z-0.125000 I-0.105499 J-0.134688
G01 X41.609375 Y44.632850 Z-0.125000
""".split("\n"), 1)
refined_point_list = interpolator.xy_list_interpolated
