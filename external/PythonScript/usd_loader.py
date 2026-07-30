from pxr import Usd, UsdGeom, Sdf, Gf
'''
method for loading geometry from a USD stage
path: path to the .usd or .usda file to be opened
returns: a list of meshes; A mesh contains: a list of vertices (points), a list of indices, a count of vertices for each face and a transformation matrix of the mesh

'''
'''
def load_meshes(paths):
    layer = Sdf.Layer.CreateNew("tmpFile.usda")
    stage = Usd.Stage.Open(layer)

    for file in paths:
        layer.subLayerPaths.append(file)
    UsdGeom.SetStageUpAxis(stage, UsdGeom.Tokens.z)

    UsdGeom.SetStageMetersPerUnit(stage, UsdGeom.LinearUnits.inches )

    stage.SetFramesPerSecond(30)
    stage.SetTimeCodesPerSecond(30)
    
    stage.SetStartTimeCode(0)
    stage.SetEndTimeCode(900)


    filename = stage.GetRootLayer().realPath

    filename = filename.replace("\\", "/")
    meshes = []

    for prim in stage.Traverse():
        if prim.IsInstanceable():
            prim.SetInstanceable(False)
        if prim.IsA(UsdGeom.Mesh):
            img = UsdGeom.Imageable(prim)
            purpose = img.GetPurposeAttr().Get()
            vis = img.GetVisibilityAttr().Get()

            if purpose not in [None, "default", "render"]:
                continue 
            if vis == "invisible":
                continue 

            mesh = UsdGeom.Mesh(prim)
            xform = UsdGeom.Xformable(prim)
            matrix = xform.ComputeLocalToWorldTransform(Usd.TimeCode.Default())

            meshes.append({
                "points": mesh.GetPointsAttr().Get(),
                "indices": mesh.GetFaceVertexIndicesAttr().Get(),
                "counts": mesh.GetFaceVertexCountsAttr().Get(),
                "matrix": matrix
            })
    return meshes
'''
'''
 prototypes = {}

    for prim in stage.Traverse():
        if prim.IsInstance():
            proto = prim.GetPrototype()
            if proto:
                prototypes[proto.GetPath()] = proto

    for prim in stage.Traverse():
        if prim.IsA(UsdGeom.Mesh):

            # skip prototype meshes here (we'll handle them separately)
            if prim.IsInPrototype():
                continue

            img = UsdGeom.Imageable(prim)
            purpose = img.GetPurposeAttr().Get()
            vis = img.GetVisibilityAttr().Get()

            if purpose not in [None, "default", "render"]:
                continue
            if vis == "invisible":
                continue

            mesh = UsdGeom.Mesh(prim)
            xform = UsdGeom.Xformable(prim)
            matrix = xform.ComputeLocalToWorldTransform(Usd.TimeCode.Default())

            meshes.append({
                "points": mesh.GetPointsAttr().Get(),
                "indices": mesh.GetFaceVertexIndicesAttr().Get(),
                "counts": mesh.GetFaceVertexCountsAttr().Get(),
                "matrix": matrix
            })

    for proto in prototypes.values():

        for prim in Usd.PrimRange(proto):
            if not prim.IsA(UsdGeom.Mesh):
                continue

            img = UsdGeom.Imageable(prim)
            purpose = img.GetPurposeAttr().Get()
            vis = img.GetVisibilityAttr().Get()

            if purpose not in [None, "default", "render"]:
                continue
            if vis == "invisible":
                continue

            mesh = UsdGeom.Mesh(prim)

            # IMPORTANT: prototypes are in local space
            matrix = UsdGeom.Xformable(proto).ComputeLocalToWorldTransform(
                Usd.TimeCode.Default()
            )

            meshes.append({
                "points": mesh.GetPointsAttr().Get(),
                "indices": mesh.GetFaceVertexIndicesAttr().Get(),
                "counts": mesh.GetFaceVertexCountsAttr().Get(),
                "matrix": matrix
            })
    return meshes
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
        if name is "":
            name = "No name given"
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