"""Run in UE 5.5 via Tools > Execute Python Script, with PIE stopped.
Generate four LODs for placed environment Static Mesh assets; save no maps.
"""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal

LOD_RATIOS = (1.0, 0.5, 0.25, 0.125)
ENVIRONMENT_ROOTS = ("/Game/Meshes/Dungeon/", "/Game/Meshes/SoulCave/Environment/")
EXCLUDED_PARTS = ("/swords/", "/weapons/", "/weapon/", "/effects/", "/fx/", "/sky/", "/water/")


def main():
    root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    maps = sorted((root / "Content" / "Levels").glob("*.umap"))
    if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():
        raise RuntimeError("Save pending editor changes before running this script.")
    output = root / "Saved" / "EnvironmentLOD" / datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    output.mkdir(parents=True)
    report = {"status": "scanning", "lod_count": 4, "ratios": LOD_RATIOS, "assets": [], "excluded": {}}
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    meshes = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    original_world = editor.get_editor_world()
    original_map = original_world.get_path_name().split(".")[0] if original_world else None

    def record():
        (output / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")

    record()
    try:
        targets = {}
        for map_file in maps:
            world = unreal.EditorLoadingAndSavingUtils.load_map(str(map_file))
            if world is None:
                raise RuntimeError("Failed to load " + str(map_file))
            for actor in actors.get_all_level_actors():
                if isinstance(actor, unreal.Pawn):
                    continue
                for component in actor.get_components_by_class(unreal.StaticMeshComponent):
                    mesh = component.get_editor_property("static_mesh")
                    if not mesh:
                        continue
                    path = mesh.get_path_name()
                    lower = path.lower()
                    if (not path.startswith(ENVIRONMENT_ROOTS)
                            or any(part in lower for part in EXCLUDED_PARTS)
                            or any(word in lower.rsplit("/", 1)[-1] for word in ("fog", "godray", "skydome"))):
                        report["excluded"][path] = "Outside environment scope or effect/weapon mesh"
                        continue
                    targets.setdefault(path, set()).add(map_file.stem)

        if not targets:
            raise RuntimeError("No placed environment meshes found; no assets changed.")
        report["status"] = "applying"
        record()
        for path in sorted(targets):
            mesh = assets.load_asset(path)
            if not isinstance(mesh, unreal.StaticMesh):
                raise RuntimeError("Expected StaticMesh: " + path)
            relative = path.split(".")[0].removeprefix("/Game/")
            # Preserve asset and any split bulk data before rebuilding.
            for suffix in (".uasset", ".ubulk", ".uexp"):
                source = root / "Content" / (relative + suffix)
                if source.exists():
                    backup = output / "backup" / (relative + suffix)
                    backup.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(source, backup)
            row = {"asset": path, "maps": sorted(targets[path]), "before": meshes.get_lod_count(mesh), "saved": False}
            report["assets"].append(row)
            record()
            options = unreal.StaticMeshReductionOptions()
            options.set_editor_property("auto_compute_lod_screen_size", True)
            settings = []
            for ratio, screen in zip(LOD_RATIOS, (1.0, 0.5, 0.2, 0.05)):
                setting = unreal.StaticMeshReductionSettings()
                setting.set_editor_property("percent_triangles", ratio)
                setting.set_editor_property("screen_size", screen)
                settings.append(setting)
            options.set_editor_property("reduction_settings", settings)
            result = meshes.set_lods(mesh, options)
            row["after"] = meshes.get_lod_count(mesh)
            if result != 4 or row["after"] != 4:
                raise RuntimeError("Four LODs were not generated: " + path)
            row["vertices"] = [meshes.get_number_verts(mesh, index) for index in range(4)]
            if any(count <= 0 for count in row["vertices"]):
                raise RuntimeError("Empty LOD generated: " + path)
            if not assets.save_loaded_asset(mesh, only_if_is_dirty=True):
                raise RuntimeError("Could not save " + path)
            row["saved"] = True
            record()
            unreal.log("[EnvironmentLOD] Saved four LODs: " + path)
        report["status"] = "complete"
        record()
        if original_map and original_map.startswith("/Game/"):
            unreal.EditorLoadingAndSavingUtils.load_map(original_map)
        unreal.log("[EnvironmentLOD] Complete. Report: " + str(output / "report.json"))
    except Exception as error:
        report["status"] = "failed"
        report["error"] = str(error)
        record()
        raise


if __name__ == "__main__":
    main()
