"""Run with Unreal Editor's PythonScript commandlet or Execute Python Script.

Caps only the identified axe textures; preserves imported source pixels.
Writes an audit report under Saved/LoadingOptimization.
"""
import json
from pathlib import Path
import unreal


def main():
    root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    output = root / "Saved" / "LoadingOptimization"
    output.mkdir(parents=True, exist_ok=True)
    report_path = output / "asset_optimization.json"
    report = {"status": "started", "textures": []}

    def write_report():
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")

    write_report()
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    try:
        # Resolve every target before changing any asset.
        textures = []
        for suffix in ("BaseColor", "Normal", "Metallic", "Roughness", "Height"):
            path = "/Game/Meshes/Weapon/Axe/textures/axe_1_low_axe_" + suffix
            texture = assets.load_asset(path)
            if not isinstance(texture, unreal.Texture2D):
                raise RuntimeError("Expected Texture2D: " + path)
            textures.append((path, texture))

        for path, texture in textures:
            previous = texture.get_editor_property("max_texture_size")
            target = min(previous, 2048) if previous > 0 else 2048
            row = {"asset": path, "previous_max_size": previous, "max_size": target}
            report["textures"].append(row)
            if previous != target:
                texture.set_editor_property("max_texture_size", target)
                if not assets.save_loaded_asset(texture, only_if_is_dirty=True):
                    raise RuntimeError("Could not save " + path)
            row["verified"] = texture.get_editor_property("max_texture_size") == target
            write_report()

        report["dungeon2_built_data_present"] = assets.does_asset_exist("/Game/Levels/Dungeon2_BuiltData")
        # Missing baked lighting cannot be reconstructed by simply resaving a map.
        report["status"] = "complete"
        write_report()
        unreal.log("Loading optimization complete: " + str(report_path))
    except Exception as error:
        report["status"] = "failed"
        report["error"] = str(error)
        write_report()
        raise


if __name__ == "__main__":
    main()
