import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


def load_module(relative_path: str, name: str):
    path = ROOT / relative_path
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"无法加载 {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Ips32GeneratorTests(unittest.TestCase):
    def test_prelaunch_patch_generator_writes_ips32_header(self):
        module = load_module(
            "tools/switch/generate_exefs_tk2d_prelaunch_patch.py",
            "generate_exefs_tk2d_prelaunch_patch",
        )
        patch = module.make_ips32(0x5B583A8, 0x94BB8FB2)
        self.assertEqual(patch[:5], b"IPS32")
        self.assertEqual(len(patch), 19)


if __name__ == "__main__":
    unittest.main()
