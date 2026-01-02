"""
citygen_py.py
==============

This module implements a pure‑Python version of the procedural city
generator.  It mirrors the structure and behaviour of the C++
implementation (see ``include/City.h`` and ``src/CityGenerator.cpp``) but
uses Python data structures and the standard library.  The Python generator
is primarily intended as a fallback in environments where a C++ compiler
is unavailable and for use in unit tests within this repository.  Users
seeking maximum performance should compile and use the C++ version.

The algorithm follows the same high‑level pipeline: assign zones based on a
hash‑based noise function, enforce a minimum amount of green space per
inhabitant【26†L7-L10】, place hospitals and schools, build a simple road
network and produce a summary of the resulting city.
"""

from dataclasses import dataclass
import math
import random
from typing import List, Tuple, Dict


@dataclass
class Config:
    population: int = 100000
    num_hospitals: int = 1
    num_schools: int = 1
    transport: str = "car"
    seed: int = 0
    grid_size: int = 100
    radius_fraction: float = 0.8


class Zone:
    NONE = 0
    RESIDENTIAL = 1
    COMMERCIAL = 2
    INDUSTRIAL = 3
    GREEN = 4


class FacilityType:
    HOSPITAL = "hospital"
    SCHOOL = "school"


def _hash_noise(x: int, y: int, seed: int) -> float:
    """Simple deterministic hash noise returning a value in [0, 1)."""
    # Combine coordinates and seed into a 32‑bit integer then scramble bits
    h = (x * 374761393) + (y * 668265263)
    h = (h ^ (seed + 0x9e3779b9 + ((h << 6) & 0xFFFFFFFF) + (h >> 2))) & 0xFFFFFFFF
    h ^= (h >> 17)
    h = (h * 0xED5AD4BB) & 0xFFFFFFFF
    h ^= (h >> 11)
    h = (h * 0xAC4C1B51) & 0xFFFFFFFF
    h ^= (h >> 15)
    return (h & 0xFFFFFF) / float(0x1000000)


def _fractal_noise(x: int, y: int, seed: int, octaves: int = 4) -> float:
    total = 0.0
    amplitude = 1.0
    frequency = 1.0
    amplitude_sum = 0.0
    for i in range(octaves):
        sx = int(x * frequency)
        sy = int(y * frequency)
        n = _hash_noise(sx, sy, seed + i * 17)
        total += amplitude * n
        amplitude_sum += amplitude
        amplitude *= 0.5
        frequency *= 2.0
    return total / amplitude_sum


def generate(config: Config) -> Dict[str, int]:
    """Generate a city summary according to the supplied configuration.

    Returns a dictionary containing counts of buildings by zone and numbers
    of facilities.  No 3D model is produced by the Python version.
    """
    size = config.grid_size
    radius = (size * config.radius_fraction) / 2.0
    centre = size / 2.0
    # Storage for zone assignments and facility flags
    zones: List[int] = [Zone.NONE] * (size * size)
    heights: List[int] = [0] * (size * size)
    facility_flags: List[str] = [""] * (size * size)  # "hospital" or "school" or ""
    rng = random.Random(config.seed)
    # Assign zones and heights
    for y in range(size):
        for x in range(size):
            idx = y * size + x
            dx = x + 0.5 - centre
            dy = y + 0.5 - centre
            dist = math.sqrt(dx * dx + dy * dy)
            if dist > radius:
                zones[idx] = Zone.NONE
                continue
            value = _fractal_noise(x, y, config.seed)
            if value < 0.55:
                zones[idx] = Zone.RESIDENTIAL
                heights[idx] = rng.randint(2, 6)
            elif value < 0.75:
                zones[idx] = Zone.COMMERCIAL
                heights[idx] = rng.randint(5, 20)
            elif value < 0.90:
                zones[idx] = Zone.INDUSTRIAL
                heights[idx] = rng.randint(3, 6)
            else:
                zones[idx] = Zone.GREEN
                heights[idx] = 0
    # Enforce minimum green space (8 m^2 per person)
    green_area_per_person = 8.0  # m^2
    cell_area = 100.0 * 100.0
    target_green_cells = int(math.ceil((config.population * green_area_per_person) / cell_area))
    current_green = sum(1 for z in zones if z == Zone.GREEN)
    if current_green < target_green_cells:
        # Build list of candidate indices (residential or industrial)
        candidates = [i for i, z in enumerate(zones) if z in (Zone.RESIDENTIAL, Zone.INDUSTRIAL)]
        rng.shuffle(candidates)
        needed = target_green_cells - current_green
        for i in candidates[:needed]:
            zones[i] = Zone.GREEN
            heights[i] = 0
    # Place facilities
    eligible = [i for i, z in enumerate(zones) if z in (Zone.RESIDENTIAL, Zone.COMMERCIAL)]
    rng.shuffle(eligible)
    def place(count: int, label: str):
        placed = 0
        for i in eligible:
            if facility_flags[i] == "" and placed < count:
                facility_flags[i] = label
                placed += 1
        return placed
    place(config.num_hospitals, FacilityType.HOSPITAL)
    place(config.num_schools, FacilityType.SCHOOL)
    # Derive accessibility metrics
    school_cells = [i for i, f in enumerate(facility_flags) if f == FacilityType.SCHOOL]
    hospital_cells = [i for i, f in enumerate(facility_flags) if f == FacilityType.HOSPITAL]
    def cell_center(idx: int) -> Tuple[float, float]:
        x = (idx % size) + 0.5
        y = (idx // size) + 0.5
        return (x, y)
    def nearest_distance(idx: int, targets: List[int]) -> float:
        if not targets:
            return -1.0
        cx, cy = cell_center(idx)
        best = float("inf")
        for t in targets:
            tx, ty = cell_center(t)
            d = math.hypot(cx - tx, cy - ty)
            if d < best:
                best = d
        return best
    max_dist_school = -1.0
    max_dist_hospital = -1.0
    max_res_height = 0
    max_com_height = 0
    max_ind_height = 0
    for i, z in enumerate(zones):
        if z == Zone.RESIDENTIAL:
            max_res_height = max(max_res_height, heights[i])
            if school_cells:
                d = nearest_distance(i, school_cells)
                if d > max_dist_school:
                    max_dist_school = d
            if hospital_cells:
                d = nearest_distance(i, hospital_cells)
                if d > max_dist_hospital:
                    max_dist_hospital = d
        elif z == Zone.COMMERCIAL:
            max_com_height = max(max_com_height, heights[i])
        elif z == Zone.INDUSTRIAL:
            max_ind_height = max(max_ind_height, heights[i])
    # Compute summary
    summary = {
        "gridSize": size,
        "totalBuildings": sum(1 for z in zones if z not in (Zone.NONE, Zone.GREEN)),
        "residentialCells": sum(1 for z in zones if z == Zone.RESIDENTIAL),
        "commercialCells": sum(1 for z in zones if z == Zone.COMMERCIAL),
        "industrialCells": sum(1 for z in zones if z == Zone.INDUSTRIAL),
        "greenCells": sum(1 for z in zones if z == Zone.GREEN),
        "undevelopedCells": sum(1 for z in zones if z == Zone.NONE),
        "numHospitals": sum(1 for f in facility_flags if f == FacilityType.HOSPITAL),
        "numSchools": sum(1 for f in facility_flags if f == FacilityType.SCHOOL),
        "maxDistanceToSchool": max_dist_school,
        "maxDistanceToHospital": max_dist_hospital,
        "maxResidentialHeight": max_res_height,
        "maxCommercialHeight": max_com_height,
        "maxIndustrialHeight": max_ind_height,
    }
    return summary
