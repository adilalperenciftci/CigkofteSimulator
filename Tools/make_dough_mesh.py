# SM_Cigkofte_Dough - the ball of bulgur the player kneads all game.
#
# It replaces /Engine/BasicShapes/Sphere, so it has to BE that sphere as far as
# the game is concerned: 100 cm across, pivot at the centre. Every number in
# ACigkofteStation::ApplyDoughTransform is relative to that, and the mesh is put
# through hard non-uniform scale - 1.30/1.30/0.60 for a loose wet mix, back to
# 1/1/1 as it comes together, times 1.20/1.20/0.70 on every stroke of the pulse.
# So the shape has to survive squashing: no sharp features, no thin parts, and
# nothing that reads as "correct" from only one axis.
#
# Colour is not baked. The game builds a dynamic material instance and drives a
# Color parameter from FCigDoughVisual::Color(), pale dry bulgur through to dark
# isot red. The mesh only needs sane UVs and smooth normals.
#
# Blender works in metres and FBX export scales by 100, so radius 0.5 here is
# the 100 cm diameter Unreal wants.

import bpy
import bmesh
import math
import random
from mathutils import Vector

RADIUS = 0.5          # 0.5 m -> 100 cm diameter in Unreal
SUBDIV = 3            # icosphere subdivisions; 3 is 1280 tris before decimate
SIT = 0.88            # vertical squash, so it rests rather than floats
SEED = 7

# Displacement in three bands, coarse to fine, and the coarse one does most of
# the work. A ball of bulgur is not a bumpy surface - it is a smooth surface
# with an outline that is never a circle, because it was pushed into shape by
# hands and then left. Loading the amplitude into the low frequency breaks the
# silhouette; the higher bands only stop it reading as an ellipsoid.
#
# Amplitudes are fractions of the radius, and they are summed, so the total
# excursion is about 0.16 either way before the mesh is normalised back to size.
BANDS = (
    (1.9, 0.105),     # frequency, amplitude - the shape of the lump itself
    (4.3, 0.038),
    (9.7, 0.016),
)

NAME = "SM_Cigkofte_Dough"


def clear_existing():
    """Remove a previous run's object so re-running is idempotent."""
    obj = bpy.data.objects.get(NAME)
    if obj:
        bpy.data.objects.remove(obj, do_unlink=True)
    for mesh in list(bpy.data.meshes):
        if mesh.users == 0:
            bpy.data.meshes.remove(mesh)


def build():
    clear_existing()
    random.seed(SEED)

    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=SUBDIV, radius=RADIUS,
                                          location=(0.0, 0.0, 0.0))
    obj = bpy.context.active_object
    obj.name = NAME
    obj.data.name = NAME

    me = obj.data
    bm = bmesh.new()
    bm.from_mesh(me)

    # Lumpiness. Sine bands rather than true noise, because they stay smooth
    # under the hard non-uniform squash the game applies - real noise develops
    # creases when you flatten it to 0.60 of its height.
    #
    # Each band gets its own axis weights and phase, so no two of them line up
    # and the result does not read as a pattern. The dot products are against
    # arbitrary directions rather than the axes, which is what stops the lumps
    # from sitting in a grid.
    dirs = []
    for _ in BANDS:
        d = Vector((random.uniform(-1.0, 1.0),
                    random.uniform(-1.0, 1.0),
                    random.uniform(-1.0, 1.0)))
        dirs.append(d.normalized())
    phases = [random.uniform(0.0, math.tau) for _ in BANDS]

    for v in bm.verts:
        n = v.co.normalized()
        wobble = 0.0
        for (freq, amp), d, ph in zip(BANDS, dirs, phases):
            # Two crossed waves per band, so a band contributes a lobe rather
            # than a stripe.
            wobble += amp * math.sin(n.dot(d) * freq + ph)
            wobble += amp * 0.6 * math.sin(n.dot(d.yzx) * freq * 1.37 + ph * 2.1)
        v.co = n * (RADIUS * (1.0 + wobble))

    # It is sitting in a tray, not hanging in the air: flatten the underside
    # more than the top, and let the mass spread very slightly as it goes down.
    for v in bm.verts:
        t = (v.co.z / RADIUS + 1.0) * 0.5          # 0 at the bottom, 1 at the top
        v.co.z *= SIT
        spread = 1.0 + 0.06 * (1.0 - t)
        v.co.x *= spread
        v.co.y *= spread

    # Back to exactly the size the game expects.
    #
    # This mesh replaces /Engine/BasicShapes/Sphere, which is 100 cm across, and
    # every number in ACigkofteStation::ApplyDoughTransform is written against
    # that. The lumps pushed the widest axis about five per cent past it, so the
    # footprint is scaled back until it measures 100 again. Height is left alone
    # - sitting lower than a sphere is the point, not an error.
    # Measured as a bounding box, not as a distance from the origin: the lumps
    # are asymmetric, so the furthest vertex being 50 cm out says nothing about
    # how wide the thing actually is.
    def extent(axis):
        vals = [v.co[axis] for v in bm.verts]
        return min(vals), max(vals)

    lo_x, hi_x = extent(0)
    lo_y, hi_y = extent(1)
    lo_z, hi_z = extent(2)

    # Centre first, so the pivot ends up in the middle of the mass. The station
    # positions this by its pivot and Unreal scales it about the same point, so
    # an off-centre pivot would make the dough drift as it is kneaded.
    cx, cy, cz = (lo_x + hi_x) * 0.5, (lo_y + hi_y) * 0.5, (lo_z + hi_z) * 0.5
    for v in bm.verts:
        v.co.x -= cx
        v.co.y -= cy
        v.co.z -= cz

    widest = max(hi_x - lo_x, hi_y - lo_y)
    if widest > 0.0:
        k = (RADIUS * 2.0) / widest
        for v in bm.verts:
            v.co.x *= k
            v.co.y *= k
            v.co.z *= k

    bm.to_mesh(me)
    bm.free()

    # Smooth shading with an autosmooth angle, so the lumps read as soft mass
    # rather than as facets - a faceted ball looks like a rock, not like dough.
    for poly in me.polygons:
        poly.use_smooth = True

    # Unwrap for the texture that will come later. Smart project is enough for
    # an organic blob and does not need seams marked by hand.
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(66.0), island_margin=0.02)
    bpy.ops.object.mode_set(mode="OBJECT")

    # Origin stays at the world centre because Unreal scales the mesh about its
    # pivot, and the station positions it by that pivot.
    obj.location = Vector((0.0, 0.0, 0.0))

    return obj


def report(obj):
    me = obj.data
    dims = obj.dimensions
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    return {
        "name": obj.name,
        "verts": len(me.vertices),
        "tris": tris,
        "size_cm": [round(d * 100.0, 1) for d in dims],
    }


result = report(build())
print("CIGKOFTE_DOUGH_RESULT", result)
