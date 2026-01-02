# Procedural City Generator

This repository contains a simple yet extensible procedural city generator
implemented primarily in C++ for performance with a Python wrapper for ease
of use.  The project demonstrates how to synthesise a three‑dimensional
model of an urban area given high‑level parameters such as population,
number of social facilities, transport mode and random seed.  The design
was inspired by academic work on city generation using noise and zoning
schemes【17†L17-L25】【18†L67-L75】 and incorporates basic urban planning
standards such as minimum green space per capita and facility placement
guidelines【25†L825-L834】【26†L7-L10】.

## Directory structure

```
city_generator_project/
├── include/        # Public C++ headers (Config.h, City.h, CityGenerator.h)
├── src/            # C++ source files implementing the generator
├── python/         # Python wrapper and helper scripts
├── tests/          # Integration tests in Python
├── docs/           # User documentation (this file)
├── paper/          # LaTeX source for the accompanying research article
└── Makefile        # Build script to compile the generator
```

## Building

To build the C++ generator, ensure you have a C++17 compiler installed
(e.g., `clang++`) and simply run:

```sh
make
```

This will produce an executable called `citygen` in the project root.

## Usage

The generator can be invoked directly from the command line.  The most
important options are the population size, number of hospitals and
schools, random seed, grid size and the radius fraction controlling the
extent of the built area.  A typical invocation is:

```sh
./citygen --population=50000 --hospitals=2 --schools=3 \
          --transport=transit --seed=42 --grid-size=100 \
          --radius-fraction=0.8 --output=out_dir
```

Upon completion the directory `out_dir` will contain two files:

- `city.obj` – a Wavefront OBJ file describing the generated 3D model.  Each
  building lot (except green spaces) is extruded into a simple cuboid.  You
  can load this file into most 3D viewers or modelling software for
  inspection.
- `city_summary.json` – a JSON document summarising key statistics such as
  the number of cells per land‑use zone, the number of facilities and the
  grid size.  This is useful for programmatic analysis and is used by the
  integration tests.

### Python interface

If you prefer to drive the generator from Python, use the wrapper in
`python/generate_city.py`.  For example:

```python
from generate_city import CityConfig, generate

cfg = CityConfig(population=75000, hospitals=3, schools=5,
                 transport="transit", seed=99,
                 grid_size=120, radius_fraction=0.9,
                 output="my_city")
generate(cfg)
```

This will call the compiled executable behind the scenes and produce
`my_city/city.obj` and `my_city/city_summary.json`.

To work with the generator outputs directly in Python, pass
``as_objects=True`` to ``generate``.  The function will parse the emitted
``city_summary.json`` into a :class:`generate_city.CitySummary` instance and
return a :class:`generate_city.CityArtifacts` container with the summary and
output paths:

```python
from generate_city import CityConfig, generate

cfg = CityConfig(output="my_city")
artefacts = generate(cfg, as_objects=True)
print(artefacts.summary.num_hospitals)
print(artefacts.model_path)
```

## Algorithm overview

The generator discretises the city into a square grid of configurable
resolution.  A fractal noise function (multi‑octave gradient noise) is
sampled at each grid coordinate to assign a land‑use zone (residential,
commercial, industrial or green).  Cells outside a specified radial
boundary remain undeveloped.  Building heights are drawn from zone‑specific
distributions (e.g. taller for commercial districts).  After zoning,
additional green cells are added if necessary to meet the commonly
recommended minimum of around 8 m² of green space per inhabitant【26†L7-L10】.

Facilities (hospitals and schools) are then placed on random residential or
commercial lots, ensuring there are exactly as many as requested.  Finally,
primary roads are constructed as a cross through the city centre and two
ring roads at fractional radii; this simplistic network can be extended
easily.  The result is serialised into both a simple 3D mesh and a summary
report.

## Testing

The `tests` directory contains integration tests executed via Python's
`unittest` framework.  To run the tests, simply execute:

```sh
python -m unittest discover -s tests -v
```

The tests compile the generator (using the top‑level Makefile) and verify
that:

1. Running the generator with identical seeds yields exactly the same
   summary (deterministic behaviour).
2. The numbers of hospitals and schools in the summary match the
   user‑supplied values.
3. The total green space allocated meets or exceeds the recommended
   minimum of 8 m² per inhabitant【26†L7-L10】.

Feel free to add further tests to cover new functionality as the
implementation evolves.

## Extensibility

This project is intended as a starting point rather than a fully
fledged simulator.  The modular C++ code can be extended in many ways:

- Improve the noise function or replace it with Perlin/Simplex noise for
  more natural patterns.
- Introduce finer zoning distinctions (e.g. medium vs high‑density
  residential), industrial subtypes or special zones (airports, stadiums).
- Generate a more realistic road network (grids, radial spokes, organic
  growth) by implementing algorithms from the literature【22†L23-L39】.
- Model building geometry more accurately, perhaps using parametric
  facades or realistic roof shapes.
- Enforce additional urban rules, such as maximum walking distance to
  schools (500 m) or transit (800 m)【25†L825-L834】.

Contributions and suggestions are welcome.
