from pxr import Usd, UsdGeom, Sdf, Gf
'''
method for loading geometry from a USD stage
path: path to the .usd or .usda file to be opened
returns: a list of meshes; A mesh contains: a list of vertices (points), a list of indices, a count of vertices for each face and a transformation matrix of the mesh

'''

def load_meshes(paths, time=Usd.TimeCode.Default()):
    layer = Sdf.Layer.CreateAnonymous(".usda")

    for path in paths:
        layer.subLayerPaths.append(path)

    stage = Usd.Stage.Open(layer)
    stage.Load()

    meshes = []

    traversal = Usd.PrimRange(
        stage.GetPseudoRoot(),
        Usd.TraverseInstanceProxies()
    )

    for prim in traversal:

        if not prim.IsA(UsdGeom.Mesh):
            continue

        imageable = UsdGeom.Imageable(prim)

        if imageable.ComputeVisibility(time) == UsdGeom.Tokens.invisible:
            continue

        purpose = imageable.ComputePurpose()

        if purpose not in (
            UsdGeom.Tokens.default_,
            UsdGeom.Tokens.render,
            None,
        ):
            continue

        mesh = UsdGeom.Mesh(prim)
        name = str(prim.GetName())
        points = mesh.GetPointsAttr().Get(time)
        counts = mesh.GetFaceVertexCountsAttr().Get(time)
        indices = mesh.GetFaceVertexIndicesAttr().Get(time)

        if not points:
            continue

        matrix = UsdGeom.Xformable(
            prim
        ).ComputeLocalToWorldTransform(time)

        meshes.append({
            "primPath": str(prim.GetPath()),
            "name": name,
            "points": points,
            "indices": indices,
            "counts": counts,
            "matrix": matrix,
            "subdivisionScheme": mesh.GetSubdivisionSchemeAttr().Get(),
        })

    return meshes