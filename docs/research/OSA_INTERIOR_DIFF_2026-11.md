# What differs between the Osa model and the photos

Written by `tools/recon/holistic_diff.py` (docs/OSA_INTERIOR_HOLISTIC.md section 32). Each finding is a region where the photo's depth or surface direction disagrees with the model from the same camera, merged across cameras. Generated; do not edit.

| Score | |
|---|---|
| Mean share of pixels agreeing | 34.1 % |
| Views compared | 19 |
| Views not registered, not used | g093-a, g096-a, g097-a, g099-b, g100-b |

## Findings, largest first

| Id | Kind | Part | Views | Pixels | Conclusion |
|---|---|---|---|---|---|
| F001 | turned | hull | g093-b, g094-a, g094-b, g095-a, g095-b | 259546 | hull: the surface is at the wrong angle by 26 degrees at x -1.04 y 1.70 z 2.21 (photo: a sloped plane). Yaw difference -1, tilt difference +26. |
| F002 | moved | hull | g100-c, g100-d | 163664 | hull: the surface should move 0.049 m along its normal at x -0.62 y -1.05 z 1.11 (x +0.003, y -0.048, z +0.007). |
| F003 | shape | rack_sp3m | g098-a, g098-b, g098-c | 116128 | rack_sp3m: the surface differs in shape at x 0.70 y 0.09 z 1.43 (median +0.003 m along the ray). |
| F004 | turned | hull | g100-a, g100-c | 144529 | hull: the surface is at the wrong angle by 17 degrees at x -0.37 y -1.05 z 1.28 (photo: a wall facing along y plane). Yaw difference +17, tilt difference +3. |
| F005 | shape | rack_sl2m | g093-b, g094-a, g094-b, g095-a, g095-b | 68370 | rack_sl2m: the surface differs in shape at x -1.09 y 1.23 z 2.06 (median -0.001 m along the ray). |
| F006 | shape | rack_sl3m | g099-d, g100-c, g100-d | 84354 | rack_sl3m: the surface differs in shape at x -0.93 y -0.75 z 0.85 (median +0.002 m along the ray). |
| F007 | excess | cabinet_0096 | g099-a | 163310 | cabinet_0096: the model stands 0.70 m in front of what the photo sees at x -0.29 y -0.33 z 1.72: too big or in the wrong place. |
| F008 | turned | hull | g092-a, g092-e, g093-c, g098-a | 60521 | hull: the surface is at the wrong angle by 16 degrees at x 0.26 y 0.33 z 1.06 (photo: a floor-like plane). Yaw difference -5, tilt difference +15. |
| F009 | moved | chest_5937 | g098-b, g098-c, g100-a | 64710 | chest_5937: the surface should move 0.050 m along its normal at x 0.55 y -0.47 z 1.35 (x +0.044, y +0.002, z -0.024). |
| F010 | turned | sp3m_face | g098-a, g098-b, g098-c | 60216 | sp3m_face: the surface is at the wrong angle by 89 degrees at x 0.46 y 0.11 z 1.97 (photo: a floor-like plane). Yaw difference +48, tilt difference -90. |
| F011 | shape | sp2m_face | g092-a, g092-e, g093-c, g098-a | 48150 | sp2m_face: the surface differs in shape at x 0.61 y 0.89 z 1.02 (median +0.000 m along the ray). |
| F012 | moved | hull | g092-a, g093-c | 80025 | hull: the surface should move 0.024 m along its normal at x 0.47 y 1.05 z 0.74 (x -0.018, y -0.009, z -0.013). |
| F013 | missing | hull | g100-a, g100-c | 77240 | hull: something stands 0.28 m in front of the model here at x -0.12 y -0.70 z 1.85: geometry the model lacks (photo: a wall facing along x surface). |
| F014 | shape | sp3m_face | g098-a, g098-b, g098-c | 46607 | sp3m_face: the surface differs in shape at x 0.70 y -0.07 z 1.49 (median -0.004 m along the ray). |
| F015 | shape | hull | g092-a, g093-c, g098-a | 45443 | hull: the surface differs in shape at x 0.38 y 0.46 z 0.92 (median -0.005 m along the ray). |
| F016 | shape | rack_sp2m | g092-a, g092-e, g093-c | 40626 | rack_sp2m: the surface differs in shape at x 0.60 y 1.89 z 1.47 (median -0.003 m along the ray). |
| F017 | moved | hull | g098-a, g100-a | 51331 | hull: the surface should move 0.073 m along its normal at x 0.43 y -0.60 z 1.01 (x +0.032, y +0.006, z -0.065). |
| F018 | excess | hull | g093-b | 76621 | hull: the model stands 0.20 m in front of what the photo sees at x -1.04 y 1.66 z 2.25: too big or in the wrong place. |
| F019 | missing | hull | g097-b | 75458 | hull: something stands 0.47 m in front of the model here at x 0.01 y 0.18 z 1.54: geometry the model lacks (photo: a wall facing along x surface). |
| F020 | shape | rack_sl3m | g093-b, g094-a, g094-b | 36432 | rack_sl3m: the surface differs in shape at x -1.05 y 0.54 z 1.43 (median -0.004 m along the ray). |
| F021 | shape | sp3m_face | g098-a, g098-b | 47463 | sp3m_face: the surface differs in shape at x 0.69 y 0.35 z 1.85 (median +0.000 m along the ray). |
| F022 | missing | hull | g099-a | 66497 | hull: something stands 0.17 m in front of the model here at x -0.79 y -0.86 z 1.73: geometry the model lacks (photo: a wall facing along x surface). |
| F023 | turned | sp2m_face | g092-a | 65252 | sp2m_face: the surface is at the wrong angle by 48 degrees at x 0.40 y 2.34 z 1.78 (photo: a sloped plane). Yaw difference +42, tilt difference -27. |
| F024 | missing | rack_sl3m | g099-d | 64915 | rack_sl3m: something stands 0.32 m in front of the model here at x -0.69 y -0.13 z 1.06: geometry the model lacks (photo: a wall facing along y surface). |
| F025 | shape | so_panel | g094-b, g095-a, g095-b | 31341 | so_panel: the surface differs in shape at x -1.10 y 1.50 z 2.03 (median +0.003 m along the ray). |
| F026 | missing | hull | g095-c | 60844 | hull: something stands 0.32 m in front of the model here at x -0.33 y 1.71 z 1.01: geometry the model lacks (photo: a sloped surface). |
| F027 | shape | rack_sl1m | g094-d, g095-b | 40079 | rack_sl1m: the surface differs in shape at x -0.90 y 2.58 z 1.14 (median +0.002 m along the ray). |
| F028 | shape | sp2m_face | g092-a, g092-e, g098-a | 29995 | sp2m_face: the surface differs in shape at x 0.61 y 1.72 z 1.85 (median -0.002 m along the ray). |
| F029 | raise | hull | g097-b | 54789 | hull: the floor here is higher in the photos by 0.11 m at x -0.53 y 0.57 z 1.06: a raised surface the model does not have. |
| F030 | turned | hull | g097-b, g099-d | 34044 | hull: the surface is at the wrong angle by 22 degrees at x -1.15 y 0.10 z 1.80 (photo: a wall facing along x plane). Yaw difference -22, tilt difference -5. |
| F031 | turned | hull | g098-a, g098-b, g100-a | 23838 | hull: the surface is at the wrong angle by 44 degrees at x 0.49 y -0.91 z 1.40 (photo: a sloped plane). Yaw difference +14, tilt difference +43. |
| F032 | missing | hull | g098-b | 45643 | hull: something stands 0.18 m in front of the model here at x 0.50 y -0.49 z 2.10: geometry the model lacks (photo: a floor-like surface). |
| F033 | excess | seat_deputy | g094-d | 44587 | seat_deputy: the model stands 0.12 m in front of what the photo sees at x -0.69 y 2.23 z 1.01: too big or in the wrong place. |
| F034 | excess | ga3 | g099-d | 44039 | ga3: the model stands 0.15 m in front of what the photo sees at x -0.94 y -0.38 z 1.06: too big or in the wrong place. |
| F035 | moved | hull | g099-a | 43458 | hull: the surface should move 0.050 m along its normal at x -0.90 y -0.41 z 2.15 (x -0.002, y +0.002, z +0.050). |
| F036 | turned | seat_deputy | g095-c | 42993 | seat_deputy: the surface is at the wrong angle by 16 degrees at x -0.54 y 2.22 z 1.17 (photo: a floor-like plane). Yaw difference +31, tilt difference +10. |
| F037 | missing | hull | g099-a | 39541 | hull: something stands 0.21 m in front of the model here at x -0.52 y -0.20 z 2.08: geometry the model lacks (photo: a floor-like surface). |
| F038 | excess | hull | g099-a | 38938 | hull: the model stands 0.32 m in front of what the photo sees at x -0.53 y -0.79 z 1.98: too big or in the wrong place. |
| F039 | excess | hull | g100-a | 38631 | hull: the model stands 0.09 m in front of what the photo sees at x -0.09 y -1.05 z 1.85: too big or in the wrong place. |
| F040 | shape | hull | g100-c, g100-d | 25285 | hull: the surface differs in shape at x -0.65 y -1.03 z 0.99 (median -0.007 m along the ray). |
| F041 | shape | rack_sl3m | g099-d, g100-c, g100-d | 18613 | rack_sl3m: the surface differs in shape at x -0.88 y -0.98 z 1.38 (median +0.002 m along the ray). |
| F042 | turned | hull | g100-c | 34865 | hull: the surface is at the wrong angle by 20 degrees at x -0.20 y -1.05 z 1.72 (photo: a wall facing along y plane). Yaw difference -2, tilt difference -20. |
| F043 | turned | sp3m_flap | g098-a, g098-b, g098-c | 16932 | sp3m_flap: the surface is at the wrong angle by 78 degrees at x 0.75 y -0.34 z 1.70 (photo: a floor-like plane). Yaw difference +5, tilt difference +78. |
| F044 | excess | hull | g094-d | 33657 | hull: the model stands 0.21 m in front of what the photo sees at x -0.50 y 2.33 z 0.62: too big or in the wrong place. |
| F045 | excess | hull | g092-e | 33575 | hull: the model stands 0.08 m in front of what the photo sees at x 0.59 y 0.62 z 0.93: too big or in the wrong place. |
| F046 | missing | hull | g099-d | 30484 | hull: something stands 0.36 m in front of the model here at x -0.54 y -0.16 z 1.14: geometry the model lacks (photo: a wall facing along y surface). |
| F047 | moved | hull | g093-c | 30296 | hull: the surface should move 0.064 m along its normal at x 0.12 y 0.49 z 0.79 (x -0.003, y -0.014, z -0.062). |
| F048 | missing | seat_senior | g095-c | 30120 | seat_senior: something stands 0.18 m in front of the model here at x -0.55 y 1.80 z 1.16: geometry the model lacks (photo: a floor-like surface). |
| F049 | raise | hull | g100-d | 28805 | hull: the floor here is higher in the photos by 0.07 m at x -0.53 y -0.86 z 0.80: a raised surface the model does not have. |
| F050 | moved | hull | g092-a | 28219 | hull: the surface should move 0.056 m along its normal at x 0.52 y 1.00 z 2.14 (x -0.013, y +0.000, z -0.055). |
| F051 | moved | hull | g094-b | 26902 | hull: the surface should move 0.031 m along its normal at x -1.04 y 0.96 z 2.13 (x -0.000, y +0.000, z +0.031). |
| F052 | excess | ga3 | g099-d | 25839 | ga3: the model stands 0.11 m in front of what the photo sees at x -0.85 y -0.24 z 1.24: too big or in the wrong place. |
| F053 | turned | rack_sl3m | g094-a, g094-b | 16448 | rack_sl3m: the surface is at the wrong angle by 25 degrees at x -1.11 y 0.48 z 1.81 (photo: a wall facing along x plane). Yaw difference -25, tilt difference -0. |
| F054 | shape | cabinet_0096 | g095-c, g097-b | 16333 | cabinet_0096: the surface differs in shape at x -0.22 y 0.43 z 1.07 (median +0.001 m along the ray). |
| F055 | excess | cabinet_0096 | g097-b | 24476 | cabinet_0096: the model stands 0.15 m in front of what the photo sees at x -0.10 y 0.30 z 0.96: too big or in the wrong place. |
| F056 | turned | seat_senior | g093-b | 23207 | seat_senior: the surface is at the wrong angle by 76 degrees at x -1.14 y 1.61 z 1.02 (photo: a wall facing along x plane). Yaw difference +16, tilt difference -104. |
| F057 | turned | seat_operator | g093-b | 22943 | seat_operator: the surface is at the wrong angle by 82 degrees at x -1.14 y 0.84 z 1.02 (photo: a wall facing along x plane). Yaw difference -39, tilt difference -95. |
| F058 | turned | seat_senior | g094-d | 22717 | seat_senior: the surface is at the wrong angle by 17 degrees at x -0.59 y 1.54 z 1.21 (photo: a floor-like plane). Yaw difference +107, tilt difference +13. |
| F059 | shape | sl1m_face | g095-b | 22345 | sl1m_face: the surface differs in shape at x -1.09 y 2.66 z 1.63 (median +0.001 m along the ray). |
| F060 | turned | sl3m_face | g099-d, g100-d | 13613 | sl3m_face: the surface is at the wrong angle by 80 degrees at x -1.01 y -0.59 z 1.54 (photo: a wall facing along y plane). Yaw difference -79, tilt difference +15. |
| F061 | shape | hull | g095-a, g095-b | 13573 | hull: the surface differs in shape at x -1.09 y 2.14 z 2.11 (median -0.017 m along the ray). |
| F062 | excess | seat_senior | g094-d | 20029 | seat_senior: the model stands 0.14 m in front of what the photo sees at x -0.86 y 1.72 z 0.99: too big or in the wrong place. |
| F063 | shape | op_panel | g094-a, g094-b, g095-c | 9918 | op_panel: the surface differs in shape at x -1.11 y 0.75 z 1.17 (median -0.013 m along the ray). |
| F064 | turned | hull | g094-d | 19529 | hull: the surface is at the wrong angle by 15 degrees at x -0.41 y 1.73 z 0.91 (photo: a wall facing along x plane). Yaw difference -1, tilt difference -15. |
| F065 | shape | hull | g093-c | 19044 | hull: the surface differs in shape at x 0.14 y 1.44 z 0.70 (median -0.007 m along the ray). |
| F066 | shape | sp3m_face | g098-b, g098-c | 11989 | sp3m_face: the surface differs in shape at x 0.85 y -0.10 z 1.83 (median +0.002 m along the ray). |
| F067 | missing | seat_operator | g095-c | 17534 | seat_operator: something stands 0.31 m in front of the model here at x -0.54 y 1.29 z 1.18: geometry the model lacks (photo: a floor-like surface). |
| F068 | excess | hull | g095-c | 17533 | hull: the model stands 0.13 m in front of what the photo sees at x -0.01 y 0.94 z 0.68: too big or in the wrong place. |
| F069 | shape | sl3m_face | g099-d, g100-d | 11206 | sl3m_face: the surface differs in shape at x -0.86 y -0.64 z 1.36 (median -0.008 m along the ray). |
| F070 | turned | so_panel | g094-a, g094-d | 10676 | so_panel: the surface is at the wrong angle by 62 degrees at x -1.16 y 1.46 z 1.10 (photo: a floor-like plane). Yaw difference +178, tilt difference +62. |
| F071 | moved | hull | g098-a | 15035 | hull: the surface should move 0.061 m along its normal at x 0.52 y -0.51 z 2.12 (x -0.009, y +0.002, z -0.060). |
| F072 | moved | seat_deputy | g095-c | 14712 | seat_deputy: the surface should move 0.059 m along its normal at x -0.83 y 2.37 z 1.07 (x -0.015, y -0.013, z -0.056). |
| F073 | missing | bench | g095-c | 13975 | bench: something stands 0.21 m in front of the model here at x -0.43 y 1.98 z 1.09: geometry the model lacks (photo: a floor-like surface). |
| F074 | turned | hull | g100-c | 13882 | hull: the surface is at the wrong angle by 30 degrees at x -0.51 y -0.68 z 1.74 (photo: a sloped plane). Yaw difference +46, tilt difference -2. |
| F075 | shape | sl3m_face | g099-a | 13864 | sl3m_face: the surface differs in shape at x -1.04 y -0.21 z 1.83 (median -0.001 m along the ray). |
| F076 | shape | rack_sl3m | g097-b, g099-d | 9193 | rack_sl3m: the surface differs in shape at x -0.88 y -0.10 z 1.44 (median +0.006 m along the ray). |
| F077 | turned | fo_panel | g094-d, g095-c | 9057 | fo_panel: the surface is at the wrong angle by 39 degrees at x -1.16 y 2.27 z 1.11 (photo: a sloped plane). Yaw difference +2, tilt difference -39. |
| F078 | raise | hull | g092-a | 13309 | hull: the floor here is higher in the photos by 0.05 m at x 0.28 y 0.95 z 0.83: a raised surface the model does not have. |
| F079 | moved | hull | g093-c, g095-c | 8784 | hull: the surface should move 0.022 m along its normal at x 0.12 y 1.74 z 0.77 (x -0.022, y +0.002, z +0.003). |
| F080 | shape | sl3m_face | g099-a, g099-d | 8355 | sl3m_face: the surface differs in shape at x -0.87 y -0.63 z 1.58 (median -0.011 m along the ray). |

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
