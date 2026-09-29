#include "simcore/atmosphere.hpp"
#include "simcore/geo.hpp"
#include "test_framework.hpp"

using namespace simcore;

SC_TEST("geo: equator/prime meridian maps to semi-major axis") {
    const Vec3 e = GeodeticToEcef(GeoPoint::FromDegrees(0.0, 0.0, 0.0));
    SC_EXPECT_NEAR(e.x, wgs84::kSemiMajorAxis, 1e-6);
    SC_EXPECT_NEAR(e.y, 0.0, 1e-6);
    SC_EXPECT_NEAR(e.z, 0.0, 1e-6);
}

SC_TEST("geo: north pole maps to semi-minor axis") {
    const Vec3 e = GeodeticToEcef(GeoPoint::FromDegrees(90.0, 0.0, 0.0));
    SC_EXPECT_NEAR(e.z, wgs84::kSemiMinorAxis, 1e-6);
}

SC_TEST("geo: geodetic <-> ECEF round trip is sub-millimetre") {
    const double lats[] = {-60.0, -12.5, 0.0, 21.0, 45.0, 67.3};
    const double lons[] = {-170.0, -45.0, 0.0, 25.0, 105.8, 179.0};
    const double alts[] = {-100.0, 0.0, 1500.0, 12000.0, 25000.0};
    for (double lat : lats)
        for (double lon : lons)
            for (double alt : alts) {
                const GeoPoint g = GeoPoint::FromDegrees(lat, lon, alt);
                const GeoPoint r = EcefToGeodetic(GeodeticToEcef(g));
                SC_EXPECT_NEAR(r.LatDeg(), lat, 1e-9);
                SC_EXPECT_NEAR(r.LonDeg(), lon, 1e-9);
                SC_EXPECT_NEAR(r.alt_m, alt, 1e-3);
            }
}

SC_TEST("geo: ENU axes point east, north and up") {
    const GeoPoint origin = GeoPoint::FromDegrees(45.0, 25.0, 200.0);
    const LocalFrame f(origin);
    const Vec3 o = f.GeodeticToEnu(origin);
    SC_EXPECT_NEAR(o.Length(), 0.0, 1e-6);

    // 0.01 degree north is ~1111 m north, ~0 east, slightly below the tangent plane.
    const Vec3 n = f.GeodeticToEnu(GeoPoint::FromDegrees(45.01, 25.0, 200.0));
    SC_EXPECT(n.y > 1100.0 && n.y < 1120.0);
    SC_EXPECT_NEAR(n.x, 0.0, 1e-3);
    SC_EXPECT(n.z < 0.0);

    const Vec3 e = f.GeodeticToEnu(GeoPoint::FromDegrees(45.0, 25.01, 200.0));
    SC_EXPECT(e.x > 780.0 && e.x < 800.0);  // 1111 m * cos(45deg)

    const Vec3 u = f.GeodeticToEnu(GeoPoint::FromDegrees(45.0, 25.0, 1200.0));
    SC_EXPECT_NEAR(u.z, 1000.0, 1e-6);
}

SC_TEST("geo: ENU <-> geodetic round trip") {
    const LocalFrame f(GeoPoint::FromDegrees(-37.8, 144.9, 50.0));
    const Vec3 p{12345.0, -54321.0, 8000.0};
    const Vec3 back = f.GeodeticToEnu(f.EnuToGeodetic(p));
    SC_EXPECT_NEAR(back.x, p.x, 1e-4);
    SC_EXPECT_NEAR(back.y, p.y, 1e-4);
    SC_EXPECT_NEAR(back.z, p.z, 1e-4);
}

SC_TEST("geo: altitude of a far ENU point includes Earth curvature") {
    const LocalFrame f(GeoPoint::FromDegrees(45.0, 25.0, 0.0));
    // 100 km along the tangent plane is ~785 m above the ellipsoid (d^2 / 2R).
    const double alt = f.AltitudeOf(Vec3{0.0, 100000.0, 0.0});
    SC_EXPECT(alt > 770.0 && alt < 800.0);
}

SC_TEST("atmosphere: ISA reference values") {
    const AtmosphereState s0 = StandardAtmosphere(0.0);
    SC_EXPECT_NEAR(s0.temperature_k, 288.15, 1e-9);
    SC_EXPECT_NEAR(s0.pressure_pa, 101325.0, 1e-6);
    SC_EXPECT_NEAR(s0.density_kg_m3, 1.2250, 5e-4);
    SC_EXPECT_NEAR(s0.speed_of_sound_mps, 340.29, 0.05);

    const AtmosphereState s11 = StandardAtmosphere(11000.0);
    SC_EXPECT_NEAR(s11.temperature_k, 216.65, 1e-9);
    SC_EXPECT_NEAR(s11.pressure_pa, 22632.0, 5.0);
    SC_EXPECT_NEAR(s11.density_kg_m3, 0.36392, 5e-4);

    const AtmosphereState s20 = StandardAtmosphere(20000.0);
    SC_EXPECT_NEAR(s20.pressure_pa, 5474.9, 5.0);
    SC_EXPECT_NEAR(s20.density_kg_m3, 0.08803, 5e-4);

    // Values at GEOPOTENTIAL altitude (the model treats geopotential == geometric).
    const AtmosphereState s30 = StandardAtmosphere(30000.0);
    SC_EXPECT_NEAR(s30.temperature_k, 226.65, 1e-9);
    SC_EXPECT_NEAR(s30.pressure_pa, 1171.87, 2.0);
}

SC_TEST("atmosphere: density decreases monotonically with altitude") {
    double prev = 1e9;
    for (double h = -500.0; h <= 40000.0; h += 250.0) {
        const double rho = StandardAtmosphere(h).density_kg_m3;
        SC_EXPECT(rho < prev);
        prev = rho;
    }
}
