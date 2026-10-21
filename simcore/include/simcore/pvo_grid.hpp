// simcore/pvo_grid.hpp — the PVO grid's squares (сетка ПВО, 1961), the form an air-situation report gives a target's
// place in (bguir_tsp: "Обработка и выдача информации неавтоматизированным способом"; pvo_bu_x_1984 art. 30: positions by
// the coded squares of the PVO grid). Proposal 9 of docs/research/PLOTTING_BOARD_2026-10.md.
//
// What the source gives: a sector is cut into big squares by parallels every 1 degree and meridians every 1 degree 30'
// (zones of types А, Б, В); a big square into nine medium squares, three rows of three, numbered 1 to 9 from the top left
// clockwise round the edge and 9 in the middle; a medium square into nine small ones numbered the same way (and a small
// one into nine "additional" squares for an eighth digit). A full place is seven digits: zone (2), sector (1), big square
// (2: its row and column in the sector), medium (1), small (1). Reports within one zone and sector may drop the zone and
// sector (the four- or five-digit form). The numbering of zones and sectors, and the coding tables that replace them for
// secrecy, are not in the source: SAM3D gives only the big square, its row and column counted from the whole degree and
// the 1 degree 30' meridian below it (each digit 0-9, a SAM3D reading), then the medium, small and additional squares.
#pragma once

#include <cmath>
#include <string>

namespace simcore {

namespace pvo_grid {

constexpr double kBigLatDeg = 1.0;
constexpr double kBigLonDeg = 1.5;

// The digit of a cell in a 3 x 3 block, row 0 at the top (north), column 0 at the left (west): 1 2 3 / 8 9 4 / 7 6 5.
inline int CellDigit(int row, int col) {
    static const int kDigits[3][3] = {{1, 2, 3}, {8, 9, 4}, {7, 6, 5}};
    return kDigits[row][col];
}
inline bool CellOf(int digit, int& row, int& col) {
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            if (CellDigit(r, c) == digit) {
                row = r;
                col = c;
                return true;
            }
        }
    }
    return false;
}

// The place's squares as digits: the big square's row and column (2), then one digit for each level below it
// (`levels`: 2 for the medium and small squares, the usual report; 3 with the additional square).
inline std::string Encode(double lat_deg, double lon_deg, int levels = 2) {
    const double lat0 = std::floor(lat_deg / kBigLatDeg) * kBigLatDeg;
    const double lon0 = std::floor(lon_deg / kBigLonDeg) * kBigLonDeg;
    const int row_digit = static_cast<int>(std::floor(lat0 / kBigLatDeg)) % 10;
    const int col_digit = static_cast<int>(std::floor(lon0 / kBigLonDeg)) % 10;
    std::string out;
    out += static_cast<char>('0' + (row_digit + 10) % 10);
    out += static_cast<char>('0' + (col_digit + 10) % 10);
    // Fractions within the big square: v up (north), u right (east).
    double u = (lon_deg - lon0) / kBigLonDeg;
    double v = (lat_deg - lat0) / kBigLatDeg;
    for (int l = 0; l < levels; ++l) {
        int col = static_cast<int>(std::floor(u * 3.0));
        int row_from_bottom = static_cast<int>(std::floor(v * 3.0));
        col = col < 0 ? 0 : (col > 2 ? 2 : col);
        row_from_bottom = row_from_bottom < 0 ? 0 : (row_from_bottom > 2 ? 2 : row_from_bottom);
        out += static_cast<char>('0' + CellDigit(2 - row_from_bottom, col));
        u = u * 3.0 - col;
        v = v * 3.0 - row_from_bottom;
    }
    return out;
}

// The centre of the squares (as a report is plotted), given the big square's corner it lies in (the whole degree and
// 1 degree 30' meridian below the place: the plotter's map shows which big square the digits mean).
inline bool Centre(const std::string& digits, double lat0_deg, double lon0_deg, double& lat_deg, double& lon_deg) {
    if (digits.size() < 3) return false;
    double u0 = 0.0, v0 = 0.0, size = 1.0;
    for (std::size_t i = 2; i < digits.size(); ++i) {
        int row = 0, col = 0;
        if (!CellOf(digits[i] - '0', row, col)) return false;
        size /= 3.0;
        u0 += col * size;
        v0 += (2 - row) * size;
    }
    lat_deg = lat0_deg + (v0 + 0.5 * size) * kBigLatDeg;
    lon_deg = lon0_deg + (u0 + 0.5 * size) * kBigLonDeg;
    return true;
}

}  // namespace pvo_grid

}  // namespace simcore
