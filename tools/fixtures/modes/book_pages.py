"""Registered synthetic fixture mode: book-pages."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='book-pages',
        fixture_args=('--book-pages-probe',),
        order=44,
        id_block=42,
        case=FixtureCase(
            name='book-pages',
            mode='book-pages',
            tests=(TestCase('test_book_pages.py', (), True, True),),
            ports={'native': 2733, 'asan': 2732},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='book-pages',
            test_args_by_variant={'asan': ('--load-only',)},
        ),
    )
)
