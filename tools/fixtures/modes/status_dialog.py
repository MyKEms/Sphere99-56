"""Registered synthetic fixture mode: status-dialog."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    from pathlib import Path
    import subprocess
    import sys

    script = Path(__file__).resolve().parents[1] / "make_status_dialog_fixture.py"
    subprocess.run([sys.executable, str(script), str(output)], check=True)
    return 0


MODE = register_mode(
    FixtureMode(
        name="status-dialog",
        fixture_args=None,
        order=94,
        id_block=94,
        case=FixtureCase(
            name="status-dialog",
            mode=None,
            tests=(TestCase("test_status_dialog.py", (), True, True),),
            ports={"native": 2894, "asan": 2895},
            mode_by_variant={},
            generator="make_status_dialog_fixture.py",
            generator_args=(),
            output="status-dialog",
            test_args_by_variant={},
        ),
    )
)
