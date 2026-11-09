# What differs between the Osa model and the photos

Written by `tools/recon/holistic_diff.py` (docs/OSA_INTERIOR_HOLISTIC.md section 32). Each finding is a region where the photo's depth or surface direction disagrees with the model from the same camera, merged across cameras. Generated; do not edit.

| Score | |
|---|---|
| Mean share of pixels agreeing | 43.4 % |
| Views compared | 11 |
| Views not registered, not used | g092-a, g093-a, g093-b, g094-b, g094-d, g095-c, g098-a, g098-c, g099-a, g099-b, g099-d, g100-b, g100-d |

## Findings, largest first

| Id | Kind | Part | Views | Pixels | Conclusion |
|---|---|---|---|---|---|
| F001 | missing | hull | g096-a, g097-a, g097-b, g098-b | 354910 | hull: something stands 0.36 m in front of the model here at x -0.20 y 0.33 z 1.32: geometry the model lacks (photo: a sloped surface). |
| F002 | turned | hull | g094-a, g095-a, g095-b | 242092 | hull: the surface is at the wrong angle by 20 degrees at x -1.02 y 1.70 z 2.21 (photo: a floor-like plane). Yaw difference -3, tilt difference +20. |
| F003 | missing | hull | g100-a, g100-c | 94220 | hull: something stands 0.26 m in front of the model here at x -0.50 y -0.77 z 1.76: geometry the model lacks (photo: a wall facing along y surface). |
| F004 | turned | cabinet_0096 | g097-a, g097-b | 87867 | cabinet_0096: the surface is at the wrong angle by 18 degrees at x -0.21 y 0.30 z 1.80 (photo: a floor-like plane). Yaw difference -5, tilt difference +18. |
| F005 | moved | hull | g093-c | 121637 | hull: the surface should move 0.062 m along its normal at x 0.32 y 0.70 z 0.75 (x -0.008, y -0.009, z -0.061). |
| F006 | shape | hull | g097-a | 98200 | hull: the surface differs in shape at x -0.13 y -0.01 z 2.24 (median -0.253 m along the ray). |
| F007 | turned | hull | g092-e, g093-c | 53096 | hull: the surface is at the wrong angle by 23 degrees at x 0.61 y 0.62 z 0.92 (photo: a wall facing along y plane). Yaw difference -23, tilt difference -9. |
| F008 | excess | seat_operator | g097-b | 58590 | seat_operator: the model stands 0.57 m in front of what the photo sees at x -0.85 y 0.42 z 1.19: too big or in the wrong place. |
| F009 | turned | hull | g100-a, g100-c | 36775 | hull: the surface is at the wrong angle by 73 degrees at x -0.40 y -1.09 z 1.35 (photo: a wall facing along x plane). Yaw difference -73, tilt difference +3. |
| F010 | excess | sl3m_face | g097-b | 49945 | sl3m_face: the model stands 0.29 m in front of what the photo sees at x -1.21 y -0.35 z 1.87: too big or in the wrong place. |
| F011 | turned | sp2m_face | g092-e | 47504 | sp2m_face: the surface is at the wrong angle by 28 degrees at x 0.55 y 1.29 z 1.39 (photo: a wall facing along x plane). Yaw difference +8, tilt difference -27. |
| F012 | excess | hull | g097-b | 45873 | hull: the model stands 0.31 m in front of what the photo sees at x -1.22 y -0.03 z 1.92: too big or in the wrong place. |
| F013 | moved | hull | g100-c | 41431 | hull: the surface should move 0.040 m along its normal at x -0.53 y -0.99 z 1.34 (x -0.000, y +0.040, z -0.000). |
| F014 | shape | rack_sl2m | g094-a, g095-a | 26306 | rack_sl2m: the surface differs in shape at x -1.09 y 1.33 z 2.06 (median -0.001 m along the ray). |
| F015 | excess | hull | g100-a | 39081 | hull: the model stands 0.09 m in front of what the photo sees at x -0.08 y -1.05 z 1.85: too big or in the wrong place. |
| F016 | moved | rack_sp2m | g092-e, g093-c | 25914 | rack_sp2m: the surface should move 0.059 m along its normal at x 0.68 y 1.10 z 0.77 (x +0.058, y +0.000, z +0.008). |
| F017 | moved | hull | g096-a | 37153 | hull: the surface should move 0.078 m along its normal at x -0.79 y 0.28 z 2.16 (x -0.009, y -0.022, z +0.074). |
| F018 | moved | hull | g093-c | 35578 | hull: the surface should move 0.067 m along its normal at x 0.54 y 1.57 z 0.58 (x -0.018, y -0.004, z -0.064). |
| F019 | moved | hull | g100-c | 35424 | hull: the surface should move 0.053 m along its normal at x -0.38 y -1.06 z 1.25 (x +0.004, y -0.053, z -0.001). |
| F020 | turned | hull | g100-c | 34532 | hull: the surface is at the wrong angle by 29 degrees at x -0.19 y -1.04 z 1.72 (photo: a wall facing along y plane). Yaw difference -3, tilt difference -29. |
| F021 | missing | rack_sl3m | g094-a, g096-a | 22943 | rack_sl3m: something stands 0.51 m in front of the model here at x -0.82 y 0.82 z 1.19: geometry the model lacks (photo: a floor-like surface). |
| F022 | shape | sp2m_face | g092-e, g093-c | 22068 | sp2m_face: the surface differs in shape at x 0.61 y 0.95 z 1.09 (median +0.009 m along the ray). |
| F023 | turned | rack_sl3m | g094-a, g096-a | 21168 | rack_sl3m: the surface is at the wrong angle by 33 degrees at x -1.10 y 0.45 z 1.63 (photo: a sloped plane). Yaw difference +33, tilt difference +3. |
| F024 | missing | hull | g097-b | 31196 | hull: something stands 0.21 m in front of the model here at x -0.58 y 0.52 z 1.07: geometry the model lacks (photo: a wall facing along x surface). |
| F025 | excess | hull | g097-b | 28719 | hull: the model stands 0.10 m in front of what the photo sees at x 0.04 y 0.33 z 0.95: too big or in the wrong place. |
| F026 | missing | hull | g096-a | 27597 | hull: something stands 0.10 m in front of the model here at x 0.48 y 0.67 z 1.03: geometry the model lacks (photo: a wall facing along y surface). |
| F027 | moved | cabinet_0096 | g096-a | 26674 | cabinet_0096: the surface should move 0.080 m along its normal at x -0.28 y 0.55 z 1.17 (x -0.022, y +0.076, z -0.011). |
| F028 | turned | hull | g098-b | 26276 | hull: the surface is at the wrong angle by 26 degrees at x 0.50 y -0.93 z 1.48 (photo: a wall facing along y plane). Yaw difference +1, tilt difference +26. |
| F029 | moved | sp2m_face | g092-e | 26144 | sp2m_face: the surface should move 0.041 m along its normal at x 0.66 y 1.24 z 0.86 (x +0.041, y +0.000, z +0.003). |
| F030 | turned | rack_sl1m | g094-a, g095-b | 17116 | rack_sl1m: the surface is at the wrong angle by 49 degrees at x -1.05 y 2.53 z 1.95 (photo: a wall facing along x plane). Yaw difference -49, tilt difference -2. |
| F031 | moved | sl1m_face | g095-b | 25433 | sl1m_face: the surface should move 0.071 m along its normal at x -1.02 y 2.55 z 1.71 (x +0.071, y +0.002, z -0.000). |
| F032 | excess | cabinet_0096 | g097-b | 25155 | cabinet_0096: the model stands 0.14 m in front of what the photo sees at x -0.16 y 0.29 z 0.95: too big or in the wrong place. |
| F033 | shape | rack_sl2m | g094-a, g095-b | 16320 | rack_sl2m: the surface differs in shape at x -1.09 y 1.90 z 2.06 (median +0.005 m along the ray). |
| F034 | moved | fix:fo_panel | g095-b | 24313 | fix:fo_panel: the surface should move 0.033 m along its normal at x -1.06 y 2.08 z 1.86 (x +0.033, y -0.000, z -0.000). |
| F035 | moved | hull | g100-a | 24290 | hull: the surface should move 0.076 m along its normal at x 0.49 y -1.03 z 1.02 (x +0.032, y +0.003, z -0.069). |
| F036 | shape | hull | g095-a, g095-b | 14573 | hull: the surface differs in shape at x -1.09 y 2.13 z 2.10 (median -0.014 m along the ray). |
| F037 | turned | rack_sp3m | g098-b | 17907 | rack_sp3m: the surface is at the wrong angle by 17 degrees at x 0.67 y -0.08 z 1.49 (photo: a sloped plane). Yaw difference +0, tilt difference -17. |
| F038 | shape | hull | g100-c | 17613 | hull: the surface differs in shape at x -0.62 y -1.02 z 1.00 (median +0.001 m along the ray). |
| F039 | moved | fix:so_panel | g095-a | 17156 | fix:so_panel: the surface should move 0.036 m along its normal at x -1.06 y 1.45 z 1.87 (x +0.036, y -0.000, z +0.000). |
| F040 | shape | sp3m_face | g098-b | 15941 | sp3m_face: the surface differs in shape at x 0.68 y 0.41 z 1.84 (median -0.001 m along the ray). |
| F041 | missing | sp3m_face | g097-a | 15867 | sp3m_face: something stands 0.54 m in front of the model here at x 0.23 y 0.35 z 1.85: geometry the model lacks (photo: a wall facing along x surface). |
| F042 | shape | so_panel | g095-a | 15852 | so_panel: the surface differs in shape at x -1.10 y 1.35 z 2.03 (median -0.000 m along the ray). |
| F043 | moved | chest_5937 | g098-b | 15736 | chest_5937: the surface should move 0.032 m along its normal at x 0.60 y -0.59 z 1.38 (x +0.004, y +0.002, z -0.032). |
| F044 | moved | hull | g100-c | 15242 | hull: the surface should move 0.037 m along its normal at x -0.20 y -1.05 z 0.90 (x +0.000, y -0.037, z +0.000). |
| F045 | turned | hull | g100-c | 15041 | hull: the surface is at the wrong angle by 32 degrees at x -0.66 y -0.95 z 0.94 (photo: a wall facing along y plane). Yaw difference -7, tilt difference +31. |
| F046 | turned | rack_sp2m | g093-c | 14430 | rack_sp2m: the surface is at the wrong angle by 16 degrees at x 0.69 y 1.47 z 0.60 (photo: a wall facing along x plane). Yaw difference -2, tilt difference -16. |
| F047 | moved | chest_5937 | g100-a | 14209 | chest_5937: the surface should move 0.070 m along its normal at x 0.58 y -1.01 z 1.28 (x +0.070, y +0.004, z -0.002). |
| F048 | shape | cabinet_0096 | g097-b | 13783 | cabinet_0096: the surface differs in shape at x -0.22 y 0.42 z 1.07 (median -0.004 m along the ray). |
| F049 | shape | sp3m_face | g096-a, g098-b | 8645 | sp3m_face: the surface differs in shape at x 0.69 y -0.09 z 2.07 (median -0.005 m along the ray). |
| F050 | turned | sp3m_face | g098-b | 12885 | sp3m_face: the surface is at the wrong angle by 19 degrees at x 0.76 y 0.23 z 1.81 (photo: a wall facing along x plane). Yaw difference +4, tilt difference +19. |
| F051 | turned | bench | g096-a | 12703 | bench: the surface is at the wrong angle by 22 degrees at x -0.66 y 1.16 z 1.08 (photo: a floor-like plane). Yaw difference +102, tilt difference -158. |
| F052 | moved | hull | g096-a | 12621 | hull: the surface should move 0.033 m along its normal at x 0.50 y 0.71 z 2.16 (x -0.011, y -0.000, z -0.031). |
| F053 | turned | hull | g096-a | 12411 | hull: the surface is at the wrong angle by 89 degrees at x -0.85 y 0.71 z 2.25 (photo: a wall facing along y plane). Yaw difference +90, tilt difference +74. |
| F054 | moved | sl3m_face | g096-a | 12005 | sl3m_face: the surface should move 0.060 m along its normal at x -0.95 y -0.08 z 1.56 (x +0.057, y +0.003, z +0.018). |
| F055 | moved | so_panel | g095-a | 11866 | so_panel: the surface should move 0.043 m along its normal at x -1.15 y 1.83 z 1.65 (x -0.043, y -0.000, z +0.000). |
| F056 | moved | fo_panel | g094-a | 11696 | fo_panel: the surface should move 0.054 m along its normal at x -1.16 y 2.42 z 1.38 (x -0.054, y +0.000, z -0.000). |
| F057 | turned | sp3m_face | g098-b | 11431 | sp3m_face: the surface is at the wrong angle by 74 degrees at x 0.74 y -0.32 z 2.07 (photo: a floor-like plane). Yaw difference -172, tilt difference -84. |
| F058 | moved | hull | g092-e | 10950 | hull: the surface should move 0.057 m along its normal at x 0.60 y 0.53 z 1.21 (x +0.001, y +0.057, z -0.000). |
| F059 | moved | rack_sl3m | g097-b | 10577 | rack_sl3m: the surface should move 0.063 m along its normal at x -0.78 y 0.13 z 1.34 (x +0.058, y -0.015, z -0.020). |
| F060 | shape | hull | g096-a | 10453 | hull: the surface differs in shape at x -0.58 y 0.65 z 2.19 (median -0.011 m along the ray). |
| F061 | shape | hull | g093-c | 9848 | hull: the surface differs in shape at x 0.24 y 1.45 z 0.72 (median -0.004 m along the ray). |
| F062 | moved | sp3m_face | g098-b | 9346 | sp3m_face: the surface should move 0.029 m along its normal at x 0.70 y -0.07 z 1.55 (x -0.006, y -0.000, z +0.028). |
| F063 | turned | sp3m_face | g098-b | 9167 | sp3m_face: the surface is at the wrong angle by 53 degrees at x 0.62 y 0.41 z 1.94 (photo: a floor-like plane). Yaw difference +24, tilt difference -50. |
| F064 | shape | hull | g093-c | 8691 | hull: the surface differs in shape at x 0.47 y 0.57 z 1.00 (median -0.004 m along the ray). |
| F065 | turned | sp3m_flap | g098-b | 8682 | sp3m_flap: the surface is at the wrong angle by 74 degrees at x 0.81 y -0.59 z 1.69 (photo: a floor-like plane). Yaw difference +4, tilt difference +74. |
| F066 | moved | hull | g100-c | 8587 | hull: the surface should move 0.043 m along its normal at x -0.03 y -1.05 z 1.29 (x +0.002, y -0.042, z +0.007). |
| F067 | turned | hull | g100-a | 8533 | hull: the surface is at the wrong angle by 13 degrees at x 0.24 y -1.03 z 1.01 (photo: a wall facing along y plane). Yaw difference -0, tilt difference +13. |
| F068 | moved | hull | g100-c | 8319 | hull: the surface should move 0.059 m along its normal at x -0.72 y -0.98 z 1.43 (x -0.000, y +0.059, z +0.000). |
| F069 | missing | sp3m_face | g096-a | 8298 | sp3m_face: something stands 0.24 m in front of the model here at x 0.69 y -0.29 z 1.77: geometry the model lacks (photo: a wall facing along x surface). |
| F070 | raise | hull | g093-c | 8059 | hull: the floor here is higher in the photos by 0.05 m at x 0.13 y 1.41 z 0.74: a raised surface the model does not have. |
| F071 | moved | hull | g100-c | 8046 | hull: the surface should move 0.047 m along its normal at x 0.19 y -0.99 z 1.70 (x -0.003, y +0.047, z -0.001). |
| F072 | shape | fix:so_panel | g095-a | 7939 | fix:so_panel: the surface differs in shape at x -1.09 y 1.70 z 1.88 (median +0.002 m along the ray). |
| F073 | shape | fo_panel | g095-b | 7724 | fo_panel: the surface differs in shape at x -1.09 y 2.08 z 1.77 (median +0.013 m along the ray). |
| F074 | turned | sp2m_face | g092-e, g093-c | 5137 | sp2m_face: the surface is at the wrong angle by 13 degrees at x 0.61 y 1.05 z 0.98 (photo: a wall facing along x plane). Yaw difference -4, tilt difference +12. |
| F075 | turned | hull | g100-c | 7137 | hull: the surface is at the wrong angle by 72 degrees at x -0.17 y -0.89 z 1.75 (photo: a floor-like plane). Yaw difference -151, tilt difference -97. |
| F076 | turned | hull | g093-c | 7064 | hull: the surface is at the wrong angle by 28 degrees at x 0.11 y 1.40 z 0.69 (photo: a sloped plane). Yaw difference -1, tilt difference -28. |
| F077 | moved | so_panel | g095-a | 7021 | so_panel: the surface should move 0.046 m along its normal at x -1.06 y 1.42 z 1.91 (x +0.046, y -0.000, z +0.000). |
| F078 | shape | fix:so_panel | g095-a | 6991 | fix:so_panel: the surface differs in shape at x -1.10 y 1.52 z 1.90 (median -0.003 m along the ray). |
| F079 | turned | hull | g093-c | 6946 | hull: the surface is at the wrong angle by 27 degrees at x 0.01 y 1.08 z 0.68 (photo: a floor-like plane). Yaw difference -35, tilt difference -26. |
| F080 | moved | fix:so_panel | g095-a | 6830 | fix:so_panel: the surface should move 0.028 m along its normal at x -1.06 y 1.46 z 1.77 (x +0.028, y -0.000, z -0.000). |

## Parts whose pose the photos would change

| Part | Fit before | Fit after | Turn (deg) | Move x, y, z (m) |
|---|---|---|---|---|
| r124_a5 | 0 % | 100 % | -28.5 | +0.110, -0.008, +0.127 |
| r124_a3 | 20 % | 98 % | +2.0 | -0.040, -0.014, +0.072 |
| pwr_cabinet | 22 % | 92 % | -23.0 | -0.023, +0.108, -0.051 |
| cgw10p | 45 % | 92 % | -18.8 | +0.083, -0.038, +0.115 |
| dome_rear | 61 % | 99 % | -16.7 | +0.050, -0.123, -0.024 |
| cabinet_0096 | 14 % | 48 % | -21.1 | -0.133, +0.096, +0.199 |
| bench | 4 % | 31 % | +10.4 | -0.144, +0.039, +0.209 |
| first_aid | 74 % | 99 % | +2.2 | +0.001, +0.013, +0.158 |
| awo5m1 | 0 % | 20 % | -8.5 | +0.270, +0.052, +0.005 |
| seat_senior | 0 % | 18 % | -40.4 | -0.213, +0.095, +0.174 |
| rack_sl3m | 12 % | 30 % | +1.9 | +0.277, +0.088, +0.036 |
| seat_operator | 24 % | 39 % | -14.5 | -0.008, -0.187, +0.160 |
| seat_deputy | 0 % | 14 % | +32.8 | -0.196, +0.154, +0.159 |
| rack_sp3m | 10 % | 23 % | -1.4 | -0.155, -0.000, -0.023 |
| sl1m_face | 3 % | 13 % | -1.3 | +0.000, -0.155, +0.099 |
