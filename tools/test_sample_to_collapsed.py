#!/usr/bin/env python3
import tempfile
import unittest
from collections import Counter
from pathlib import Path

import sample_to_collapsed as s2c


class SampleToCollapsedTests(unittest.TestCase):
    def test_parse_file_collapses_simple_stack(self) -> None:
        sample_text = """Call graph:
    1 main + 100
      2 update + 50
        3 render + 25
"""
        with tempfile.TemporaryDirectory() as td:
            p = Path(td) / "sample.txt"
            p.write_text(sample_text, encoding="utf-8")
            out = Counter()
            s2c.parse_file(p, out)

            self.assertIn("main", out)
            self.assertIn("update;main", out)
            self.assertIn("render;update;main", out)
            self.assertEqual(out["main"], 1)
            self.assertEqual(out["update;main"], 1)
            self.assertEqual(out["render;update;main"], 1)


if __name__ == "__main__":
    unittest.main()
