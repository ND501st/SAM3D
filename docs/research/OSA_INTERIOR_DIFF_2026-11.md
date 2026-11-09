# What differs between the Osa model and the photos

Written by `tools/recon/holistic_diff.py` (docs/OSA_INTERIOR_HOLISTIC.md section 32). Each finding is a region where the photo's depth or surface direction disagrees with the model from the same camera, merged across cameras. Generated; do not edit.

| Score | |
|---|---|
| Mean share of pixels agreeing | 28.6 % |
| Views compared | 21 |
| Views not registered, not used | g096-a, g097-a, g099-b |

## Findings, largest first

| Id | Kind | Part | Views | Pixels | Conclusion |
|---|---|---|---|---|---|
| F001 | turned | hull | g093-b, g094-a, g094-b, g095-a, g095-b | 259549 | hull: the surface is at the wrong angle by 26 degrees at x -1.04 y 1.70 z 2.21 (photo: a sloped plane). Yaw difference -1, tilt difference +26. |
| F002 | moved | hull | g092-a, g092-e, g093-c, g098-a | 160875 | hull: the surface should move 0.055 m along its normal at x 0.28 y 0.85 z 0.74 (x +0.006, y -0.008, z -0.054). |
| F003 | turned | hull | g092-a, g092-e, g098-a, g098-b, g098-c | 131365 | hull: the surface is at the wrong angle by 22 degrees at x 0.70 y 0.31 z 1.49 (photo: a sloped plane). Yaw difference +28, tilt difference +3. |
| F004 | missing | hull | g099-a, g100-b | 262214 | hull: something stands 0.17 m in front of the model here at x -0.48 y 0.66 z 2.03: geometry the model lacks (photo: a floor-like surface). |
| F005 | missing | hull | g099-a, g100-a, g100-c | 149162 | hull: something stands 0.16 m in front of the model here at x -0.77 y -0.87 z 1.70: geometry the model lacks (photo: a wall facing along x surface). |
| F006 | moved | hull | g100-c, g100-d | 163664 | hull: the surface should move 0.049 m along its normal at x -0.62 y -1.05 z 1.11 (x +0.003, y -0.048, z +0.007). |
| F007 | turned | hull | g100-a, g100-c | 144529 | hull: the surface is at the wrong angle by 17 degrees at x -0.37 y -1.05 z 1.28 (photo: a wall facing along y plane). Yaw difference +17, tilt difference +3. |
| F008 | shape | rack_sl2m | g093-b, g094-a, g094-b, g095-a, g095-b | 68370 | rack_sl2m: the surface differs in shape at x -1.09 y 1.23 z 2.06 (median -0.001 m along the ray). |
| F009 | excess | seat_deputy | g094-d, g095-c | 134450 | seat_deputy: the model stands 0.33 m in front of what the photo sees at x -0.77 y 2.11 z 1.23: too big or in the wrong place. |
| F010 | shape | rack_sp3m | g098-a, g098-b, g098-c | 86730 | rack_sp3m: the surface differs in shape at x 0.70 y -0.04 z 1.44 (median +0.004 m along the ray). |
| F011 | shape | rack_sl3m | g099-d, g100-c, g100-d | 84187 | rack_sl3m: the surface differs in shape at x -0.93 y -0.75 z 0.85 (median +0.002 m along the ray). |
| F012 | excess | cabinet_0096 | g099-a | 146342 | cabinet_0096: the model stands 0.65 m in front of what the photo sees at x -0.28 y -0.27 z 1.72: too big or in the wrong place. |
| F013 | excess | seat_senior | g093-a | 133638 | seat_senior: the model stands 0.67 m in front of what the photo sees at x -0.42 y 1.03 z 1.07: too big or in the wrong place. |
| F014 | raise | hull | g093-a | 124186 | hull: the floor here is higher in the photos by 0.40 m at x -0.38 y 1.69 z 1.47: a raised surface the model does not have. |
| F015 | excess | seat_senior | g093-b | 121293 | seat_senior: the model stands 0.60 m in front of what the photo sees at x -1.10 y 1.76 z 1.27: too big or in the wrong place. |
| F016 | shape | sp2m_face | g092-a, g092-e, g093-c, g098-a | 47248 | sp2m_face: the surface differs in shape at x 0.61 y 0.89 z 1.02 (median +0.000 m along the ray). |
| F017 | shape | sp3m_face | g098-a, g098-b, g098-c | 57231 | sp3m_face: the surface differs in shape at x 0.69 y -0.19 z 2.05 (median -0.004 m along the ray). |
| F018 | shape | hull | g092-a, g093-c, g098-a | 49685 | hull: the surface differs in shape at x 0.38 y 0.48 z 0.93 (median -0.010 m along the ray). |
| F019 | moved | hull | g098-a, g098-b | 60048 | hull: the surface should move 0.072 m along its normal at x 0.54 y -0.45 z 2.11 (x -0.012, y -0.000, z -0.071). |
| F020 | excess | hull | g100-b | 83537 | hull: the model stands 0.33 m in front of what the photo sees at x -0.78 y -0.20 z 2.37: too big or in the wrong place. |
| F021 | shape | rack_sp2m | g092-a, g092-e, g093-c | 40551 | rack_sp2m: the surface differs in shape at x 0.60 y 1.89 z 1.46 (median -0.003 m along the ray). |
| F022 | turned | seat_deputy | g093-a | 78634 | seat_deputy: the surface is at the wrong angle by 36 degrees at x -0.64 y 2.25 z 1.52 (photo: a sloped plane). Yaw difference +36, tilt difference +19. |
| F023 | excess | hull | g093-b | 76621 | hull: the model stands 0.20 m in front of what the photo sees at x -1.04 y 1.66 z 2.25: too big or in the wrong place. |
| F024 | missing | hull | g097-b | 75459 | hull: something stands 0.47 m in front of the model here at x 0.01 y 0.18 z 1.54: geometry the model lacks (photo: a wall facing along x surface). |
| F025 | moved | hull | g098-a, g100-a | 50173 | hull: the surface should move 0.065 m along its normal at x 0.42 y -0.62 z 1.00 (x +0.030, y +0.005, z -0.058). |
| F026 | turned | sp3m_flap | g093-a, g098-a, g098-b, g098-c | 29032 | sp3m_flap: the surface is at the wrong angle by 75 degrees at x 1.48 y -1.40 z 1.57 (photo: a floor-like plane). Yaw difference -144, tilt difference +71. |
| F027 | missing | hull | g095-c | 71015 | hull: something stands 0.28 m in front of the model here at x -0.31 y 1.80 z 0.97: geometry the model lacks (photo: a wall facing along x surface). |
| F028 | turned | sp2m_face | g092-a | 65007 | sp2m_face: the surface is at the wrong angle by 48 degrees at x 0.40 y 2.34 z 1.78 (photo: a sloped plane). Yaw difference +42, tilt difference -27. |
| F029 | turned | sp3m_face | g098-b, g098-c | 42530 | sp3m_face: the surface is at the wrong angle by 89 degrees at x 0.46 y 0.11 z 1.97 (photo: a floor-like plane). Yaw difference +48, tilt difference -90. |
| F030 | excess | seat_operator | g093-b | 63295 | seat_operator: the model stands 0.55 m in front of what the photo sees at x -1.12 y 0.75 z 1.27: too big or in the wrong place. |
| F031 | shape | so_panel | g094-b, g095-a, g095-b | 31341 | so_panel: the surface differs in shape at x -1.10 y 1.50 z 2.03 (median +0.003 m along the ray). |
| F032 | shape | sp2m_face | g092-a, g092-e, g098-a | 30003 | sp2m_face: the surface differs in shape at x 0.61 y 1.72 z 1.86 (median -0.002 m along the ray). |
| F033 | excess | seat_senior | g094-d | 56427 | seat_senior: the model stands 0.22 m in front of what the photo sees at x -0.66 y 1.52 z 1.20: too big or in the wrong place. |
| F034 | missing | hull | g099-d | 55888 | hull: something stands 0.32 m in front of the model here at x -0.58 y -0.15 z 1.06: geometry the model lacks (photo: a wall facing along y surface). |
| F035 | turned | bench | g094-d, g095-c | 35925 | bench: the surface is at the wrong angle by 65 degrees at x -0.42 y 2.02 z 1.11 (photo: a sloped plane). Yaw difference -179, tilt difference -22. |
| F036 | excess | seat_operator | g100-b | 53143 | seat_operator: the model stands 1.29 m in front of what the photo sees at x 0.53 y 0.35 z 2.16: too big or in the wrong place. |
| F037 | excess | seat_operator | g093-a | 47670 | seat_operator: the model stands 0.90 m in front of what the photo sees at x -0.44 y 0.18 z 0.89: too big or in the wrong place. |
| F038 | excess | seat_operator | g097-b | 45949 | seat_operator: the model stands 0.62 m in front of what the photo sees at x -0.92 y 0.36 z 1.24: too big or in the wrong place. |
| F039 | missing | rack_sl3m | g099-d | 45154 | rack_sl3m: something stands 0.31 m in front of the model here at x -0.72 y -0.17 z 1.30: geometry the model lacks (photo: a wall facing along y surface). |
| F040 | shape | sp3m_face | g098-a, g098-b | 30069 | sp3m_face: the surface differs in shape at x 0.69 y 0.38 z 1.79 (median -0.004 m along the ray). |
| F041 | excess | hull | g099-a | 45064 | hull: the model stands 0.35 m in front of what the photo sees at x -0.49 y -0.81 z 1.96: too big or in the wrong place. |
| F042 | turned | cabinet_0096 | g093-a | 43897 | cabinet_0096: the surface is at the wrong angle by 81 degrees at x -0.33 y 0.96 z 1.48 (photo: a floor-like plane). Yaw difference +57, tilt difference +72. |
| F043 | excess | ga3 | g099-d | 43639 | ga3: the model stands 0.15 m in front of what the photo sees at x -0.94 y -0.38 z 1.07: too big or in the wrong place. |
| F044 | moved | hull | g099-a | 43154 | hull: the surface should move 0.051 m along its normal at x -0.89 y -0.42 z 2.15 (x -0.002, y +0.002, z +0.051). |
| F045 | excess | hull | g100-a | 38631 | hull: the model stands 0.09 m in front of what the photo sees at x -0.09 y -1.05 z 1.85: too big or in the wrong place. |
| F046 | raise | hull | g097-b | 38170 | hull: the floor here is higher in the photos by 0.12 m at x -0.46 y 0.55 z 1.02: a raised surface the model does not have. |
| F047 | shape | hull | g100-c, g100-d | 25285 | hull: the surface differs in shape at x -0.65 y -1.03 z 0.99 (median -0.007 m along the ray). |
| F048 | shape | rack_sl3m | g099-d, g100-c, g100-d | 18613 | rack_sl3m: the surface differs in shape at x -0.88 y -0.98 z 1.38 (median +0.002 m along the ray). |
| F049 | moved | chest_5937 | g098-c | 35066 | chest_5937: the surface should move 0.050 m along its normal at x 0.55 y -0.47 z 1.35 (x +0.044, y +0.002, z -0.025). |
| F050 | turned | hull | g100-c | 34865 | hull: the surface is at the wrong angle by 20 degrees at x -0.20 y -1.05 z 1.72 (photo: a wall facing along y plane). Yaw difference -2, tilt difference -20. |
| F051 | turned | sl3m_face | g099-a, g099-d, g100-d | 17128 | sl3m_face: the surface is at the wrong angle by 80 degrees at x -1.01 y -0.59 z 1.54 (photo: a wall facing along y plane). Yaw difference -79, tilt difference +15. |
| F052 | turned | hull | g094-d | 34211 | hull: the surface is at the wrong angle by 12 degrees at x -0.40 y 1.75 z 0.92 (photo: a wall facing along x plane). Yaw difference -1, tilt difference -12. |
| F053 | shape | rack_sl3m | g094-a, g094-b | 22623 | rack_sl3m: the surface differs in shape at x -1.05 y 0.54 z 1.43 (median -0.004 m along the ray). |
| F054 | excess | hull | g092-e | 31498 | hull: the model stands 0.08 m in front of what the photo sees at x 0.59 y 0.62 z 0.92: too big or in the wrong place. |
| F055 | turned | hull | g098-a, g098-b, g100-a | 15556 | hull: the surface is at the wrong angle by 57 degrees at x 0.53 y -0.94 z 1.43 (photo: a sloped plane). Yaw difference +35, tilt difference +48. |
| F056 | shape | rack_sl1m | g094-d, g095-b | 20331 | rack_sl1m: the surface differs in shape at x -1.09 y 2.56 z 1.57 (median +0.003 m along the ray). |
| F057 | turned | hull | g098-a | 29035 | hull: the surface is at the wrong angle by 15 degrees at x 0.27 y 0.33 z 1.08 (photo: a floor-like plane). Yaw difference +7, tilt difference +15. |
| F058 | raise | hull | g100-d | 28805 | hull: the floor here is higher in the photos by 0.07 m at x -0.53 y -0.86 z 0.80: a raised surface the model does not have. |
| F059 | moved | hull | g092-a | 28229 | hull: the surface should move 0.055 m along its normal at x 0.52 y 0.99 z 2.14 (x -0.012, y +0.000, z -0.054). |
| F060 | excess | hull | g097-b | 27846 | hull: the model stands 0.24 m in front of what the photo sees at x -1.16 y 0.10 z 1.78: too big or in the wrong place. |
| F061 | moved | hull | g094-b | 26902 | hull: the surface should move 0.031 m along its normal at x -1.04 y 0.96 z 2.13 (x -0.000, y +0.000, z +0.031). |
| F062 | shape | sp3m_face | g098-b, g098-c | 17876 | sp3m_face: the surface differs in shape at x 0.85 y -0.07 z 1.84 (median +0.005 m along the ray). |
| F063 | moved | hull | g100-b | 25112 | hull: the surface should move 0.048 m along its normal at x 0.17 y 0.58 z 2.28 (x -0.015, y -0.011, z +0.044). |
| F064 | turned | rack_sl3m | g094-a, g094-b | 16448 | rack_sl3m: the surface is at the wrong angle by 25 degrees at x -1.11 y 0.48 z 1.81 (photo: a wall facing along x plane). Yaw difference -25, tilt difference -0. |
| F065 | shape | cabinet_0096 | g095-c, g097-b | 16332 | cabinet_0096: the surface differs in shape at x -0.22 y 0.43 z 1.07 (median +0.001 m along the ray). |
| F066 | excess | cabinet_0096 | g097-b | 24478 | cabinet_0096: the model stands 0.15 m in front of what the photo sees at x -0.10 y 0.30 z 0.96: too big or in the wrong place. |
| F067 | turned | cabinet_0096 | g093-a | 23791 | cabinet_0096: the surface is at the wrong angle by 75 degrees at x -0.18 y 0.18 z 1.65 (photo: a floor-like plane). Yaw difference -69, tilt difference +50. |
| F068 | shape | sl1m_face | g095-b | 22345 | sl1m_face: the surface differs in shape at x -1.09 y 2.66 z 1.63 (median +0.001 m along the ray). |
| F069 | turned | ga3 | g099-d | 22090 | ga3: the surface is at the wrong angle by 12 degrees at x -0.85 y -0.25 z 1.26 (photo: a wall facing along x plane). Yaw difference -11, tilt difference +5. |
| F070 | turned | chest_5937 | g098-a, g098-b | 14460 | chest_5937: the surface is at the wrong angle by 13 degrees at x 0.54 y -0.57 z 1.33 (photo: a wall facing along x plane). Yaw difference +4, tilt difference -12. |
| F071 | turned | seat_senior | g095-c | 20796 | seat_senior: the surface is at the wrong angle by 79 degrees at x -0.66 y 1.43 z 1.24 (photo: a floor-like plane). Yaw difference +151, tilt difference +80. |
| F072 | shape | hull | g095-a, g095-b | 13573 | hull: the surface differs in shape at x -1.09 y 2.14 z 2.11 (median -0.017 m along the ray). |
| F073 | shape | chest_5937 | g098-a, g098-b | 13064 | chest_5937: the surface differs in shape at x 0.67 y -0.46 z 1.41 (median -0.083 m along the ray). |
| F074 | turned | hull | g093-c | 18277 | hull: the surface is at the wrong angle by 46 degrees at x 0.37 y 1.61 z 0.63 (photo: a floor-like plane). Yaw difference +127, tilt difference -19. |
| F075 | turned | hull | g098-a, g098-b, g100-a | 8946 | hull: the surface is at the wrong angle by 21 degrees at x 0.30 y -1.04 z 1.99 (photo: a wall facing along y plane). Yaw difference +0, tilt difference +21. |
| F076 | shape | sl3m_face | g099-d, g100-d | 11206 | sl3m_face: the surface differs in shape at x -0.86 y -0.64 z 1.36 (median -0.008 m along the ray). |
| F077 | moved | hull | g092-a, g098-a | 11005 | hull: the surface should move 0.044 m along its normal at x 0.48 y 0.40 z 1.09 (x +0.039, y -0.010, z -0.017). |
| F078 | excess | hull | g095-c | 16317 | hull: the model stands 0.13 m in front of what the photo sees at x -0.02 y 0.93 z 0.68: too big or in the wrong place. |
| F079 | shape | sp3m_flap | g093-a, g098-c | 10585 | sp3m_flap: the surface differs in shape at x 0.69 y -0.62 z 1.65 (median -0.002 m along the ray). |
| F080 | turned | rack_sp3m | g093-a | 15754 | rack_sp3m: the surface is at the wrong angle by 83 degrees at x 0.11 y 0.75 z 1.48 (photo: a floor-like plane). Yaw difference -143, tilt difference +52. |

## Parts whose pose the photos would change

| Part | Fit before | Fit after | Turn (deg) | Move x, y, z (m) |
|---|---|---|---|---|
| r124_a2 | 0 % | 100 % | -2.1 | +0.001, -0.201, +0.110 |
| ga3 | 20 % | 80 % | -45.9 | +0.007, +0.056, +0.261 |
| seat_operator | 22 % | 54 % | +2.3 | +0.169, +0.065, +0.213 |
| bench | 23 % | 55 % | -8.0 | -0.054, -0.083, +0.262 |
| seat_deputy | 49 % | 67 % | +10.7 | -0.070, -0.032, +0.112 |
| awo5m1 | 7 % | 26 % | -6.3 | -0.078, +0.004, +0.164 |
| rack_sl3m | 28 % | 42 % | +14.7 | +0.189, +0.016, +0.007 |
