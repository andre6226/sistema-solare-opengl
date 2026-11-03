#!/usr/bin/env python3
"""Sunlight that Earth's atmosphere bends into its shadow, for the Moon in a lunar eclipse.

For each tangent height h (0-60 km, 1 km steps) of a ray grazing Earth's limb:
  - delta(h): total refraction angle, through the US Standard Atmosphere 1976;
  - T(h): transmission of sunlight along the tangent path, Rayleigh + ozone (Chappuis)
    + tropospheric aerosol, times the share of the limb clear of clouds, integrated over
    the spectrum (5772 K Sun, CIE 1931) and given in linear sRGB relative to sunlight,
    which the shader treats as white.
base.frag adds up, for a point in the shadow, the limb ring that shows it the Sun.

    python3 tools/umbra_light.py            # prints the table and checks
    python3 tools/umbra_light.py --write    # rewrites the table in SolarSystem/base.frag
"""
import math
import pathlib
import sys

R_EARTH = 6371.0          # km
H_MAX, H_STEP = 60, 1     # km; above 60 km the bend is under 0.1" and the light is direct

# --- US Standard Atmosphere 1976: layers (base km, base K, lapse K/km) up to 86 km.
LAYERS = [(0, 288.15, -6.5), (11, 216.65, 0.0), (20, 216.65, 1.0), (32, 228.65, 2.8),
          (47, 270.65, 0.0), (51, 270.65, -2.8), (71, 214.65, -2.0), (86, 186.87, 0.0)]
G0_M_OVER_R = 34.1632     # g0 M / R*, K/km


def temperature_and_pressure(z):
    """Temperature (K) and pressure (relative to sea level) at geopotential height z km."""
    p = 1.0
    for i, (zb, tb, lapse) in enumerate(LAYERS):
        ztop = LAYERS[i + 1][0] if i + 1 < len(LAYERS) else 1e9
        dz = min(z, ztop) - zb
        if lapse == 0.0:
            p *= math.exp(-G0_M_OVER_R * dz / tb)
        else:
            p *= (tb / (tb + lapse * dz)) ** (G0_M_OVER_R / lapse)
        if z <= ztop:
            return tb + lapse * dz, p
    raise AssertionError


def density(z):
    """Air density relative to sea level; exponential above 86 km (scale height 6 km)."""
    if z > 86.0:
        return density(86.0) * math.exp(-(z - 86.0) / 6.0)
    t, p = temperature_and_pressure(max(z, 0.0))
    return p * 288.15 / t


N0_REFRACTIVITY = 2.77e-4      # n - 1 of sea-level air at 15 C, 600 nm
N_AIR0 = 2.547e19              # molecules per cm^3 at sea level, 15 C

# shortcut: ozone as a Gaussian layer (300 DU, peak 22 km, sigma 6 km) and its Chappuis
# cross-sections read by hand off Serdyuchenko et al. 2014 at 293 K; use the full
# tabulated spectrum and a climatological profile if eclipse colours must match a given year.
OZONE_COLUMN = 300 * 2.687e16  # molecules per cm^2
OZONE_PEAK_KM, OZONE_SIGMA_KM = 22.0, 6.0
O3_XSEC = [(380, 0.0), (420, 0.03), (450, 0.25), (475, 0.75), (500, 1.5), (525, 2.3),
           (550, 3.3), (575, 4.6), (600, 5.1), (625, 4.4), (650, 2.9), (675, 1.9),
           (700, 1.0), (750, 0.4), (780, 0.25)]   # 1e-21 cm^2

# Tropospheric aerosol: vertical depth 0.1 at 550 nm, scale height 1.2 km, Angstrom 1.3.
# shortcut: no volcanic stratospheric aerosol; after Pinatubo-like eruptions eclipses
# are far darker (Danjon L 0-1).
AEROSOL_TAU550, AEROSOL_H, AEROSOL_ANGSTROM = 0.10, 1.2, 1.3


def ozone_density(z):
    """Ozone molecules per cm^3 at height z km."""
    peak = OZONE_COLUMN / (OZONE_SIGMA_KM * 1e5 * math.sqrt(2 * math.pi))
    return peak * math.exp(-0.5 * ((z - OZONE_PEAK_KM) / OZONE_SIGMA_KM) ** 2)


def clear_of_clouds(h):
    """Share of the limb whose tangent ray at h km passes above every cloud top.
    shortcut: one global mean (low clouds everywhere, tops thinning out to ~14 km);
    real eclipses swing with the weather along the terminator."""
    x = min(max((h - 2.0) / 12.0, 0.0), 1.0)
    return x * x * (3 - 2 * x)


def rayleigh_xsec(lam_nm):
    """Rayleigh cross-section per molecule, cm^2 (Bucholtz 1995 fit above 0.5 um, close enough below)."""
    lam = lam_nm / 1000.0
    return 4.01e-28 / lam ** (3.916 + 0.074 * lam + 0.050 / lam)


def o3_xsec(lam_nm):
    pts = O3_XSEC
    for (l0, s0), (l1, s1) in zip(pts, pts[1:]):
        if lam_nm <= l1:
            return (s0 + (s1 - s0) * (lam_nm - l0) / (l1 - l0)) * 1e-21
    return pts[-1][1] * 1e-21


def tangent_columns(h):
    """Air (relative-density km), ozone (cm^-2) and aerosol (relative km) along the whole
    tangent path at height h, as a straight line (the bend changes it by under 1%)."""
    air = o3 = aer = 0.0
    r0 = R_EARTH + h
    ds = 0.5
    s = ds / 2
    while s < 2000.0:
        z = math.sqrt(r0 * r0 + s * s) - R_EARTH
        air += density(z) * ds
        o3 += ozone_density(z) * ds * 1e5
        aer += math.exp(-z / AEROSOL_H) * ds
        s += ds
    return 2 * air, 2 * o3, 2 * aer


def refraction(h):
    """Total bend (rad) of a ray tangent at h km, both halves of the path:
    2 * integral of -d(ln n)/dr * a0 / sqrt(a^2 - a0^2) dr, a = n r (Bouguer's invariant)."""
    n = lambda z: 1.0 + N0_REFRACTIVITY * density(z)
    r0 = R_EARTH + h
    a0 = n(h) * r0
    total = 0.0
    # r = r0 + u^2 removes the square-root singularity at the tangent point.
    du = 0.005
    u = du / 2
    while u < 15.0:
        r = r0 + u * u
        z = r - R_EARTH
        dndr = (n(z + 0.01) - n(z - 0.01)) / 0.02
        a = n(z) * r
        total += -dndr / n(z) * a0 / math.sqrt(max(a * a - a0 * a0, 1e-30)) * 2 * u * du
        u += du
    return 2 * total


# --- Colour: CIE 1931 (Wyman, Sloan & Shirley 2013 fit), 5772 K Sun, linear sRGB (D65).
def _g(x, m, s1, s2):
    s = s1 if x < m else s2
    return math.exp(-0.5 * ((x - m) / s) ** 2)


def cmf(l):
    x = 1.056 * _g(l, 599.8, 37.9, 31.0) + 0.362 * _g(l, 442.0, 16.0, 26.7) - 0.065 * _g(l, 501.1, 20.4, 26.2)
    y = 0.821 * _g(l, 568.8, 46.9, 40.5) + 0.286 * _g(l, 530.9, 16.3, 31.1)
    z = 1.217 * _g(l, 437.0, 11.8, 36.0) + 0.681 * _g(l, 459.0, 26.0, 13.8)
    return x, y, z


def planck(l_nm, t=5772.0):
    l = l_nm * 1e-9
    return 1.0 / (l ** 5 * (math.exp(1.4388e-2 / (l * t)) - 1.0))


def to_rgb(xyz):
    x, y, z = xyz
    return (3.2406 * x - 1.5372 * y - 0.4986 * z,
            -0.9689 * x + 1.8758 * y + 0.0415 * z,
            0.0557 * x - 0.2040 * y + 1.0570 * z)


WAVELENGTHS = range(380, 781, 5)


def transmission_rgb(h):
    air, o3, aer = tangent_columns(h)
    clear = clear_of_clouds(h)
    sun = [0.0, 0.0, 0.0]
    seen = [0.0, 0.0, 0.0]
    for l in WAVELENGTHS:
        tau = (rayleigh_xsec(l) * N_AIR0 * air * 1e5 + o3_xsec(l) * o3
               + AEROSOL_TAU550 / AEROSOL_H * (l / 550.0) ** -AEROSOL_ANGSTROM * aer)
        s = planck(l)
        c = cmf(l)
        for i in range(3):
            sun[i] += s * c[i]
            seen[i] += s * c[i] * math.exp(-tau) * clear
    a, b = to_rgb(seen), to_rgb(sun)
    return tuple(max(a[i], 0.0) / b[i] for i in range(3))   # out-of-gamut deep red: clamp at 0


def table():
    return [(h, *transmission_rgb(h), refraction(h)) for h in range(0, H_MAX + 1, H_STEP)]


def umbra_irradiance(rows, rho, d_km, alpha):
    """What base.frag computes: sunlight share (RGB) reaching a point rho rad off the shadow
    axis, d_km from Earth, the Sun alpha rad in radius. Radiance is conserved along a ray,
    so the point gets the limb's solid angle that, traced back and bent by delta, lands on
    the Sun's disc."""
    e = [0.0, 0.0, 0.0]
    for h, tr, tg, tb, delta in rows:
        b = delta - math.asin((R_EARTH + h) / d_km)
        if rho * abs(b) < 1e-12:
            arc = 2 * math.pi if rho * rho + b * b <= alpha * alpha else 0.0
        else:
            c = (alpha * alpha - rho * rho - b * b) / (2 * rho * abs(b))
            arc = 2 * math.pi - 2 * math.acos(min(max(c, -1.0), 1.0))
        w = arc * (R_EARTH + h) * H_STEP / (d_km * d_km * math.pi * alpha * alpha)
        for i, t in enumerate((tr, tg, tb)):
            e[i] += t * w
    return e


def self_check(rows):
    deg = math.degrees
    # Refraction at the horizon is ~35' each way, so ~65-70' grazing; it falls with the air, 1/e per ~8-9 km near the ground.
    assert 60 < deg(rows[0][4]) * 60 < 80, deg(rows[0][4]) * 60
    assert 0.35 < rows[8][4] / rows[0][4] < 0.55, rows[8][4] / rows[0][4]
    # Light through the limb: redder and fainter the lower it goes, near 1 at 60 km.
    assert rows[3][1] < rows[10][1] < rows[30][1] <= 1.0
    assert rows[12][1] > rows[12][2] > rows[12][3]
    assert min(rows[60][1:4]) > 0.95
    # At the Moon's mean distance the umbra's centre gets 1e-6..1e-2 of sunlight (mid-totality
    # Moons are 1e-4 to 1e-6 of the full Moon), red;
    # its edge far more: the dark centre and bright copper rim of photos.
    d, alpha = 384400.0, math.radians(0.2666)
    centre = umbra_irradiance(rows, 0.0, d, alpha)
    rim = umbra_irradiance(rows, 4300.0 / d, d, alpha)
    assert 1e-6 < centre[0] < 1e-2 and centre[0] > 3 * centre[1], centre
    assert rim[0] > 3 * centre[0], (rim, centre)


def glsl(rows):
    lines = ["// tools/umbra_light.py: per km of tangent height 0-60 km, sunlight through Earth's",
             "// limb (linear sRGB, sunlight = 1) and its total refraction (rad). Generated, do not edit.",
             f"const int UMBRA_ROWS = {len(rows)};",
             "const vec4 UMBRA_LIMB[UMBRA_ROWS] = vec4[]("]
    lines += [f"    vec4({tr:.4e}, {tg:.4e}, {tb:.4e}, {d:.6e}){',' if i + 1 < len(rows) else ''}"
              for i, (h, tr, tg, tb, d) in enumerate(rows)]
    lines.append(");")
    return "\n".join(lines)


def main():
    rows = table()
    d, alpha = 384400.0, math.radians(0.2666)
    for h, tr, tg, tb, delta in rows[::5]:
        print(f"h {h:2d} km  bend {math.degrees(delta) * 60:6.2f}'  T rgb {tr:.3e} {tg:.3e} {tb:.3e}")
    for r in (0, 1000, 2000, 3000, 4000, 4300, 4600, 5000):
        e = umbra_irradiance(rows, r / d, d, alpha)
        print(f"r {r:4d} km  E rgb {e[0]:.2e} {e[1]:.2e} {e[2]:.2e}")
    self_check(rows)
    if "--write" in sys.argv:
        path = pathlib.Path(__file__).resolve().parent.parent / "SolarSystem" / "base.frag"
        src = path.read_text()
        begin, end = "// <umbra-table>\n", "// </umbra-table>\n"
        i, j = src.index(begin) + len(begin), src.index(end)
        path.write_text(src[:i] + glsl(rows) + "\n" + src[j:])
        print("wrote", path)


if __name__ == "__main__":
    main()
