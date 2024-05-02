# Sensor interval volume, mesured with a syringe
sensor_volume_cc = 3.0

renesis_single_face_max_volume_cc = 654.0
compression_ratio = 10 # 10:1
renesis_single_face_min_volume_cc = renesis_single_face_max_volume_cc / 10
volume_min_with_sensor = renesis_single_face_min_volume_cc + sensor_volume_cc
dead_space_correction_factor = volume_min_with_sensor / renesis_single_face_min_volume_cc

print(f"DEAD_SPACE_CORRECTION_FACTOR: {dead_space_correction_factor}")
