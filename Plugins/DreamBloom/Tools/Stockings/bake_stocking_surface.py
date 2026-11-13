"""Run inside Unreal Editor: Tools > Execute Python Script.

Select one independent stocking Skeletal Mesh in the Content Browser, or set
STOCKING_ASSET to its /Game/... asset path below. This is not a Blender script.
"""
import unreal

# Leave empty to use the Skeletal Mesh selected in the Content Browser.
STOCKING_ASSET = globals().get("STOCKING_ASSET", "")
BODY_ASSET = globals().get("BODY_ASSET", "/Game/Model/Daniya/Red/DaniyaToUe")
# Leave empty to create /Game/Wardrobe/Stockings/DA_<mesh name>_Surface.
OUTPUT_ASSET = globals().get("OUTPUT_ASSET", "")
ALLOW_OVERWRITE = globals().get("ALLOW_OVERWRITE", False)
# True checks selection, dependencies and output path without writing assets.
VALIDATE_ONLY = globals().get("VALIDATE_ONLY", False)
# Set False when running in UnrealEditor-Cmd without an editor UI.
SYNC_CONTENT_BROWSER = globals().get("SYNC_CONTENT_BROWSER", True)


def bake():
    tools = getattr(unreal, "WardrobeStockingEditorTools", None)
    if tools is None:
        raise RuntimeError("DreamBloomEditor is not loaded. Compile the plugin and open this project in Unreal Editor.")

    if STOCKING_ASSET:
        stocking = unreal.load_asset(STOCKING_ASSET)
    else:
        selected = list(unreal.EditorUtilityLibrary.get_selected_assets())
        if len(selected) != 1 or not isinstance(selected[0], unreal.SkeletalMesh):
            raise RuntimeError("Select exactly one independent stocking Skeletal Mesh in the Content Browser, then run this script again.")
        stocking = selected[0]
    if not isinstance(stocking, unreal.SkeletalMesh):
        raise RuntimeError("STOCKING_ASSET must point to an imported Skeletal Mesh: " + STOCKING_ASSET)

    body = unreal.load_asset(BODY_ASSET)
    if not isinstance(body, unreal.SkeletalMesh):
        raise RuntimeError("The character body Skeletal Mesh was not found: " + BODY_ASSET)
    if stocking == body:
        raise RuntimeError("Choose the independent stocking mesh, not the character body.")

    output = OUTPUT_ASSET or "/Game/Wardrobe/Stockings/DA_" + stocking.get_name() + "_Surface"
    if not output.startswith("/Game/") or output.endswith("/") or "." in output or " " in output:
        raise RuntimeError("OUTPUT_ASSET must be a package path such as /Game/Wardrobe/Stockings/DA_MyStockingSurface, without .uasset.")
    if unreal.EditorAssetLibrary.does_asset_exist(output):
        existing = unreal.load_asset(output)
        if not isinstance(existing, unreal.WardrobeStockingSurface):
            raise RuntimeError("Output already contains a different asset type. Choose another OUTPUT_ASSET: " + output)
        if not ALLOW_OVERWRITE:
            raise RuntimeError("Surface already exists. Set ALLOW_OVERWRITE = True to rebake it, or choose another OUTPUT_ASSET: " + output)

    unreal.log("STOCKING_BAKE_INPUT mesh=" + stocking.get_path_name() + " body=" + body.get_path_name() + " output=" + output)
    if VALIDATE_ONLY:
        unreal.log("STOCKING_BAKE_VALIDATED: no assets written.")
        return None

    # BakeSurface saves the generated Surface and the independent source mesh/skeleton.
    # It also clears the stocking mesh's Physics Asset because picking uses triangles.
    surface = tools.bake_surface(stocking, body, output)
    if not surface:
        raise RuntimeError("Baking failed. Check the Output Log for STOCKING_BAKE / STOCKING_IMPORT_ALIGNMENT errors. The current baker requires Daniya-compatible leg bones and reference pose.")
    if not unreal.EditorAssetLibrary.save_loaded_asset(surface):
        raise RuntimeError("The Surface was created but could not be saved: " + output)
    vertex_count = len(surface.get_editor_property("vertices"))
    triangle_count = len(surface.get_editor_property("triangles")) // 3
    unreal.log("STOCKING_BAKE_COMPLETE surface=" + output + " vertices=" + str(vertex_count) + " triangles=" + str(triangle_count))
    unreal.log("Set DA_WardrobeCharacters > Characters > Stockings > Surface to this asset; keep M_StockingPull as Material and enable Enabled.")
    if SYNC_CONTENT_BROWSER:
        unreal.EditorAssetLibrary.sync_browser_to_objects([output])
    return surface


bake()
