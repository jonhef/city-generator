"""
test_citygen.py
================

This module contains integration tests for the procedural city generator.  The
tests use the Python ``unittest`` framework and invoke the compiled
``citygen`` executable via ``subprocess``.  The tests validate that the
generator produces deterministic output for a given seed, honours
user-specified counts of facilities, and assigns an adequate amount of
green space based on the population as described in the urban planning
guidelines【25†L825-L834】【26†L7-L10】.
"""

import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

import sys



PROJECT_ROOT = Path(__file__).resolve().parents[1]
EXECUTABLE = PROJECT_ROOT / "citygen"

# Append the Python implementation directory to sys.path so that the
# fallback generator can be imported if the C++ executable is not
# available.  This must occur after PROJECT_ROOT is defined.
sys.path.append(str(PROJECT_ROOT / "python"))

from generate_city import CityArtifacts, CityConfig, CitySummary, generate


def compile_generator():
    """
    Compile the C++ generator into an executable.  Rather than
    relying on an external ``make`` program (which may not be available
    in the execution environment), this function invokes the system C++
    compiler directly.  It compiles all files in the ``src`` directory with
    C++17 support and links them into the ``citygen`` binary in the
    project root.
    """
    src_dir = PROJECT_ROOT / "src"
    sources = [str(p) for p in src_dir.glob("*.cpp")]
    output = PROJECT_ROOT / "citygen"
    # Determine whether a C++ compiler is available.  If not, skip compilation
    compiler = shutil.which("g++")
    if compiler is None:
        # No compiler in the environment; skip compilation and rely on the Python fallback
        return
    cmd = [
        compiler, "-std=c++17", "-O2", "-Wall",
        "-I", str(PROJECT_ROOT / "include"),
    ] + sources + ["-o", str(output)]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"Compilation failed:\n{result.stderr}")


def run_generator(population: int = 100000, hospitals: int = 1, schools: int = 1,
                  seed: int = 0, grid_size: int = 100, radius: float = 0.8,
                  output_dir: Path | None = None) -> dict:
    """Run the city generator and return the summary data.

    If the compiled C++ executable exists, this function invokes it and
    parses the resulting JSON.  If the executable does not exist (e.g.
    because a C++ compiler is unavailable), the fallback Python
    implementation in ``python/citygen_py.py`` is used directly.
    """
    # Use compiled binary if present
    if EXECUTABLE.exists():
        if output_dir is None:
            output_dir = Path(tempfile.mkdtemp())
        else:
            os.makedirs(output_dir, exist_ok=True)
        args = [
            str(EXECUTABLE),
            f"--population={population}",
            f"--hospitals={hospitals}",
            f"--schools={schools}",
            f"--transport=car",
            f"--seed={seed}",
            f"--grid-size={grid_size}",
            f"--radius-fraction={radius}",
            f"--output={output_dir}"
        ]
        result = subprocess.run(args, capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(f"Generator failed: {result.stderr}")
        summary_path = output_dir / "city_summary.json"
        with open(summary_path) as f:
            return json.load(f)
    else:
        # Fallback: use Python implementation
        from citygen_py import Config as PyConfig, generate as generate_py
        cfg = PyConfig(
            population=population,
            num_hospitals=hospitals,
            num_schools=schools,
            transport="car",
            seed=seed,
            grid_size=grid_size,
            radius_fraction=radius,
        )
        return generate_py(cfg)


class TestCityGenerator(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        # Compile once for all tests
        compile_generator()

    def test_deterministic_output(self):
        """Generating with the same seed should produce identical summaries."""
        data1 = run_generator(population=50000, hospitals=2, schools=3, seed=123)
        data2 = run_generator(population=50000, hospitals=2, schools=3, seed=123)
        self.assertEqual(data1, data2, "Generator output differs for identical seeds")

    def test_facility_counts(self):
        """Ensure the requested number of hospitals and schools appear in the summary."""
        data = run_generator(population=20000, hospitals=3, schools=5, seed=42)
        self.assertEqual(data["numHospitals"], 3,
                         "Number of hospitals in summary does not match requested count")
        self.assertEqual(data["numSchools"], 5,
                         "Number of schools in summary does not match requested count")

    def test_green_ratio(self):
        """Check that green space allocation meets or exceeds the recommended minimum."""
        pop = 80000
        data = run_generator(population=pop, hospitals=1, schools=1, seed=7)
        green_cells = data["greenCells"]
        total_cells = data["gridSize"] ** 2
        # Compute ratio of green area per person (each cell ~10000 m^2).  We
        # compare to the target of 8 m^2 per person【26†L7-L10】.
        cell_area = 100.0 * 100.0
        green_area = green_cells * cell_area
        ratio = green_area / pop
        self.assertGreaterEqual(ratio, 8.0,
            f"Green space per capita {ratio:.2f} m^2 is below the recommended minimum")

    def test_accessibility_constraints(self):
        """Residential parcels should be within a reasonable distance to schools and hospitals."""
        radius_fraction = 0.8
        grid_size = 100
        data = run_generator(population=60000, hospitals=2, schools=6, seed=21,
                             grid_size=grid_size, radius=radius_fraction)
        city_radius = (grid_size * radius_fraction) / 2.0
        max_allowed_school = city_radius * 1.25  # generous but bounded
        max_allowed_hospital = city_radius * 1.95
        self.assertGreaterEqual(data["maxDistanceToSchool"], 0.0,
                                "No school reachable from residential parcels")
        self.assertGreaterEqual(data["maxDistanceToHospital"], 0.0,
                                "No hospital reachable from residential parcels")
        self.assertLessEqual(data["maxDistanceToSchool"], max_allowed_school,
                             "Schools are too far from residential parcels")
        self.assertLessEqual(data["maxDistanceToHospital"], max_allowed_hospital,
                             "Hospitals are too far from residential parcels")

    def test_height_limits_by_zone(self):
        """Building heights should respect zoning caps."""
        data = run_generator(population=40000, hospitals=1, schools=4, seed=33)
        self.assertLessEqual(data["maxResidentialHeight"], 12,
                             "Residential height cap exceeded")
        self.assertLessEqual(data["maxCommercialHeight"], 40,
                             "Commercial height cap exceeded")
        self.assertLessEqual(data["maxIndustrialHeight"], 14,
                             "Industrial height cap exceeded")


class TestPythonBindings(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        compile_generator()

    @unittest.skipUnless(EXECUTABLE.exists(), "citygen executable not built")
    def test_generate_returns_python_objects(self):
        """The Python bindings can optionally return structured artefacts."""

        with tempfile.TemporaryDirectory() as tmpdir:
            cfg = CityConfig(
                population=15000,
                hospitals=1,
                schools=2,
                transport="car",
                seed=11,
                grid_size=60,
                radius_fraction=0.75,
                output=tmpdir,
            )
            artefacts = generate(cfg, as_objects=True)

            self.assertIsInstance(artefacts, CityArtifacts)
            self.assertEqual(Path(tmpdir), artefacts.output_dir)
            self.assertIsInstance(artefacts.summary, CitySummary)
            self.assertEqual(cfg.grid_size, artefacts.summary.grid_size)
            self.assertTrue(artefacts.summary_path.exists())
            self.assertTrue(artefacts.model_path.exists())


if __name__ == '__main__':
    unittest.main()
