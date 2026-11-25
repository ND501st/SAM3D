"""Tests for tools/check_assets.py: third-party art that may not be redistributed is never tracked and always ignored."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import check_assets  # noqa: E402

FAB = {"id": "barn_fab", "unreal_path": "/Game/ThirdParty/Farm/SM_Barn.SM_Barn", "licence": "Fab-Standard",
       "redistributable": False, "origin": "https://www.fab.com/", "fetched": "2026-11-02"}


def ignored_third_party(path):
    return path.startswith("unreal/SAM3D/Content/ThirdParty/") or path.startswith("art/thirdparty/")


def test_content_file_maps_an_object_path_to_its_uasset():
    assert check_assets.content_file("/Game/ThirdParty/Farm/SM_Barn.SM_Barn") == "unreal/SAM3D/Content/ThirdParty/Farm/SM_Barn.uasset"
    assert check_assets.content_file("C:/art/barn.fbx") is None


def test_an_ignored_untracked_asset_passes():
    assert check_assets.problems({"assets": [FAB]}, set(), ignored_third_party) == []


def test_a_tracked_or_unignored_asset_fails():
    tracked = {"unreal/SAM3D/Content/ThirdParty/Farm/SM_Barn.uasset"}
    found = check_assets.problems({"assets": [FAB]}, tracked, ignored_third_party)
    assert any("tracked" in p for p in found)
    found = check_assets.problems({"assets": [FAB]}, set(), lambda p: False)
    assert any("not ignored" in p for p in found)


def test_anything_tracked_under_the_third_party_folders_fails_even_unregistered():
    found = check_assets.problems({"assets": []}, {"art/thirdparty/trees/oak.fbx"}, ignored_third_party)
    assert len(found) == 1 and "tracked" in found[0]


def test_the_licence_rules():
    outside = dict(FAB, unreal_path="/Game/SAM3D/Map/SM_Barn.SM_Barn")
    assert any("not under /Game/ThirdParty/" in p for p in check_assets.problems({"assets": [outside]}, set(), ignored_third_party))
    ccby = dict(FAB, licence="CC-BY-4.0", redistributable=True, author="")
    assert any("author" in p for p in check_assets.problems({"assets": [ccby]}, set(), ignored_third_party))
    odd = dict(FAB, licence="royalty free")
    assert any("licence" in p for p in check_assets.problems({"assets": [odd]}, set(), ignored_third_party))


def test_models_and_munitions_name_library_ids_and_registered_assets():
    ccby = dict(FAB, id="mig21_sf", licence="CC-BY-4.0", redistributable=True, author="someone")
    libraries = {"models": {"mig21"}, "munitions": {"mk82"}}
    good = {"assets": [ccby], "models": {"mig21": "mig21_sf"}, "munitions": {"mk82": None}}
    assert check_assets.problems(good, set(), ignored_third_party, libraries) == []
    unknown = {"assets": [ccby], "models": {"mig99": "mig21_sf"}}
    assert any("not in data/scenery/basic_models.json" in p for p in check_assets.problems(unknown, set(), ignored_third_party, libraries))
    unregistered = {"assets": [ccby], "munitions": {"mk82": "mk82_sf"}}
    assert any("not registered" in p for p in check_assets.problems(unregistered, set(), ignored_third_party, libraries))


def test_the_repository_passes():
    assert check_assets.main() == 0
