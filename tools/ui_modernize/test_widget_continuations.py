"""Physical preprocessor gates independent of paint/semantic normalization.

Native UI_ParseMenu stops silently at a top-level closing brace.  A missing
continuation in a widget #define can expose exactly that brace, even when the
semantic validator (which joins logical source) accepts the same item fields.
"""
from pathlib import Path
import re
import unittest

import layout_common
import restyle
from semantic import strip_comments, tokens


ROOT = Path(__file__).resolve().parents[2]


def physical_macros(text):
    """Check complete widget macros using physical lines before joining them.

This intentionally does not use restyle.blocks/item_prop or strip backslashes
from the entire file.  Every leaked non-directive token, including a standalone
`}`, is rejected.  Strings and comments cannot alter the brace balance.
    """
    result = []
    name = None
    start = 0
    body = []
    for number, line in enumerate(strip_comments(text).splitlines(), 1):
        define = re.match(r'^\s*#\s*define\s+([A-Za-z_]\w*)(.*)$', line)
        directive = re.match(r'^\s*#\s*[A-Za-z_]\w*\b', line)
        if name is None:
            if define:
                name, remainder = define.groups()
                start = number
                body = [remainder]
            elif directive or not line.strip():
                continue
            else:
                leaked = tokens(line)
                raise ValueError('Standalone token outside widget #define at line %d: %r' %
                                 (number, leaked[0] if leaked else line))
        else:
            if directive:
                raise ValueError('Directive swallowed by continuing macro %s at line %d' %
                                 (name, number))
            body.append(line)

        if re.search(r'\\[ \t]*$', line):
            continue

        # This macro has ended physically. It must contain balanced items and
        # action braces without borrowing a closing brace from the next line.
        logical = '\n'.join(re.sub(r'\\[ \t]*$', '', part) for part in body)
        depth = 0
        for token in tokens(logical):
            if token == '{':
                depth += 1
            elif token == '}':
                depth -= 1
                if depth < 0:
                    raise ValueError('Unmatched closing brace in macro %s at line %d' % (name, start))
        if depth:
            raise ValueError('Unclosed brace in macro %s at line %d; missing continuation' % (name, start))
        result.append((name, start, logical))
        name = None
        body = []

    if name is not None:
        raise ValueError('Unterminated continuation in macro %s at line %d' % (name, start))
    return result


class WidgetContinuationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Exercise the real production generator against the immutable original
        # widget input; no files are regenerated or changed by these tests.
        cls.original = restyle.read(ROOT / 'pak/ui/menumacros.h')
        cls.styled = restyle.macros(cls.original)
        cls.generated = layout_common.widgets(cls.styled)

    def test_original_and_generated_headers_have_only_complete_macros(self):
        original = physical_macros(self.original)
        generated = physical_macros(self.generated)
        self.assertGreater(len(original), 30)
        self.assertEqual([name for name, _, _ in original], [name for name, _, _ in generated])
        self.assertEqual(physical_macros(restyle.read(ROOT / 'ui-modern/ui/modern_widgets.h')),
                         physical_macros(layout_common.widgets(restyle.read(ROOT / 'ui-modern/ui/menumacros.h'))))

    def test_exact_inserted_subwindow_title_bordercolor_regression_is_rejected(self):
        start = self.generated.index('#define SUBWINDOWBLACK(')
        end = self.generated.index('\n#define ', start + 1)
        macro = self.generated[start:end]
        self.assertIn('"subwindowblacktitle"', macro)
        # Recreate the historical item_prop insertion: a new bordercolor line
        # immediately before the final item brace, without its trailing slash.
        bad, count = re.subn(r'(?m)^(\s*bordercolor\s+0 0 0 0)[ \t]*\\[ \t]*(\n[ \t]*})',
                            r'\1\2', macro)
        self.assertEqual(count, 1)
        candidate = self.generated[:start] + bad + self.generated[end:]
        with self.assertRaisesRegex(ValueError, 'Unclosed brace.*SUBWINDOWBLACK'):
            physical_macros(candidate)

    def test_a_bare_top_level_closing_brace_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Standalone token.*outside widget'):
            physical_macros('}\n' + self.generated)

    def test_missing_continuation_in_an_existing_rect_line_is_rejected(self):
        candidate, count = re.subn(r'(?m)^(\s*rect[^\n]*?)[ \t]*\\[ \t]*$',
                                  r'\1', self.generated, count=1)
        self.assertEqual(count, 1)
        with self.assertRaisesRegex(ValueError, 'Unclosed brace'):
            physical_macros(candidate)

    def test_continuation_cannot_swallow_the_next_definition(self):
        with self.assertRaisesRegex(ValueError, 'Directive swallowed'):
            physical_macros('#define FIRST 1 \\\n#define SECOND 2\n')

    def test_quoted_and_commented_braces_do_not_create_false_alarms(self):
        good = ('// A comment containing } and {\n'
                '#define EXAMPLE(LABEL) \\\n'
                ' itemDef { \\\n'
                '  name "literal } {" /* } */ \\\n'
                '  text LABEL \\\n'
                '  action { exec "echo }" ; } \\\n'
                ' }\n')
        self.assertEqual([name for name, _, _ in physical_macros(good)], ['EXAMPLE'])


if __name__ == '__main__':
    unittest.main()
