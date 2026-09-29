"""samproto — pure-Python prototype of the SAM3D sim core (runs anywhere, incl. the Mac)."""
from .engagement import AcmiWriter, Aircraft, Missile, MissileParams, Outcome, World
from .geo import GeoPoint, LocalFrame, standard_atmosphere
from .radar import RadarParams, detection_range, evaluate_detection, flat, radar_horizon, snr_db
from .vec import Vec3

__all__ = [
    "AcmiWriter", "Aircraft", "GeoPoint", "LocalFrame", "Missile", "MissileParams", "Outcome",
    "RadarParams", "Vec3", "World", "detection_range", "evaluate_detection", "flat",
    "radar_horizon", "snr_db", "standard_atmosphere",
]
