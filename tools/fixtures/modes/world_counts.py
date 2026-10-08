"""Registered synthetic fixture mode: world-counts."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    result = generate_recipe(output, MODE)
    if result:
        return result

    # The compatibility recipe creates a hair item to exercise @Create.  Put
    # that item on the fixture map so the unplaced-NEWITEM diagnostic remains
    # reserved for the dedicated cleanup probe.
    scripts = output / "scripts" / "spheretables.scp"
    text = scripts.read_text(encoding="ascii")
    text = text.replace(
        "NEWITEM SYNTHETIC_HAIR\n",
        "NEWITEM SYNTHETIC_HAIR\nLASTNEW.P=128,128,0\n",
        1,
    )
    scripts.write_text(text, encoding="ascii")
    return 0


MODE = register_mode(
    FixtureMode(
        name='world-counts',
        fixture_args=('--world-load-counts', '--world-load-counts-probe'),
        order=7,
        id_block=7,
        case=FixtureCase(
            name='world-counts',
            mode='world-counts',
            tests=(TestCase('test_world_load_counts.py', (), True, True),),
            ports={'native': 2712, 'asan': 2710},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='world-counts',
            test_args_by_variant={},
        ),
    )
)
