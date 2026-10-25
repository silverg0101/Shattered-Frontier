"""CPU schema/contract checks; actual shader rendering is an Unreal gate."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from action_target_outline import load_settings, shader


class OutlineContractTests(unittest.TestCase):
    def test_shipped_renderer_contract(self):
        settings = load_settings(ROOT / 'content/interaction/selection-render.json')
        header = (ROOT / 'GameClient/Source/ShatteredFrontier/Public/Interaction/SFActionTargetComponent.h').read_text()
        self.assertIn(f'SelectionStencilBit = {settings.stencil_bit};', header)
        self.assertIn('SceneTextureFetch(PPI_CustomStencil', shader(settings))
        self.assertIn('SceneTextureFetch(PPI_CustomDepth', shader(settings))
        self.assertIn('r.CustomDepth=3', (ROOT / 'GameClient/Config/DefaultEngine.ini').read_text())

    def test_reject_invalid_or_unversioned_defaults(self):
        source = json.loads((ROOT / 'content/interaction/selection-render.json').read_text())
        for key, value in [('schema', True), ('stencil_bit', 1), ('radius_pixels', 20), ('opacity', float('nan')),
                           ('occlusion_tolerance_cm', -1), ('color_linear_rgb', [1, 2, 3]), ('unknown', 1)]:
            with self.subTest(key=key), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / 'bad.json'; path.write_text(json.dumps({**source, key: value}))
                with self.assertRaises(ValueError): load_settings(path)


if __name__ == '__main__': unittest.main()
