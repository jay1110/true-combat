"""ET collision patch surface grids, independent of the renderer tessellation.

Grid subdivision follows CM_GeneratePatchCollide / CM_SubdivideGridColumns:
quadratic midpoint deviation 16 units, duplicate-column epsilon 0.1, then
transpose and repeat. Native facet border planes, bevels and tolerances are
not reproduced by this triangle surface representation. Validate movement
against the running game's CM traces before accepting navigation.
"""

import math


SUBDIVIDE_DISTANCE = 16.0
POINT_EPSILON = 0.1
MAX_GRID_SIZE = 129


def _mid(a, b):
    return tuple((x + y) * 0.5 for x, y in zip(a, b))


def _subdivide_columns(grid):
    grid = [list(column) for column in grid]
    i = 0
    while i < len(grid) - 2:
        triples = list(zip(grid[i], grid[i + 1], grid[i + 2]))
        curved = any(math.dist(_mid(_mid(a, b), _mid(b, c)), _mid(a, c))
                     >= SUBDIVIDE_DISTANCE for a, b, c in triples)
        if not curved:
            del grid[i + 1]
            i += 1
        else:
            if len(grid) + 2 > MAX_GRID_SIZE:
                raise ValueError("collision patch subdivision exceeds MAX_GRID_SIZE")
            left = [_mid(a, b) for a, b, c in triples]
            right = [_mid(b, c) for a, b, c in triples]
            center = [_mid(a, b) for a, b in zip(left, right)]
            grid[i + 1:i + 2] = [left, center, right]
    i = 0
    while i + 1 < len(grid):
        if all(all(abs(x - y) <= POINT_EPSILON for x, y in zip(a, b))
               for a, b in zip(grid[i], grid[i + 1])):
            del grid[i + 1]
        else:
            i += 1
    return grid


def collision_patch_grid(width, height, points):
    if (width < 3 or height < 3 or not width % 2 or not height % 2
            or width > MAX_GRID_SIZE or height > MAX_GRID_SIZE
            or len(points) != width * height
            or not all(len(p) == 3 and all(map(math.isfinite, p)) for p in points)):
        raise ValueError("invalid collision patch control grid")
    grid = [[tuple(points[j * width + i]) for j in range(height)]
            for i in range(width)]
    grid = _subdivide_columns(grid)
    grid = _subdivide_columns(list(map(list, zip(*grid))))
    return grid


def collision_patch_triangles(width, height, points):
    grid = collision_patch_grid(width, height, points)
    # CM keeps the grid transposed and PlaneFromPoints uses cross(c-a,b-a).
    # Reverse its triples for conventional OBJ/Recast cross(b-a,c-a) normals.
    triangles = []
    for i in range(len(grid) - 1):
        for j in range(len(grid[0]) - 1):
            a, b = grid[i][j], grid[i + 1][j]
            c, d = grid[i + 1][j + 1], grid[i][j + 1]
            for tri in ((a, c, b), (c, a, d)):
                u = tuple(x - y for x, y in zip(tri[1], tri[0]))
                v = tuple(x - y for x, y in zip(tri[2], tri[0]))
                normal = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2],
                          u[0]*v[1]-u[1]*v[0])
                if sum(x*x for x in normal) > 1e-12:
                    triangles.append(tri)
    return triangles, (len(grid), len(grid[0]) if grid else 0)
