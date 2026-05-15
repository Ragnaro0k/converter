from pxr import Usd, UsdGeom, Sdf
'''
method for loading geometry from a USD stage
path: path to the .usd or .usda file to be opened
returns: a list of meshes; A mesh contains: a list of vertices (points), a list of indices, a count of vertices for each face and a transformation matrix of the mesh

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