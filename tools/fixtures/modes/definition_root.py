"""Registered synthetic fixture mode: definition-root properties and STRFIRSTCAP."""

from pathlib import Path

from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "DefinitionRootProbe"
PASSWORD = "defroot-pw"
EVENT_NAME = "e_DefinitionRootProbe"
MARKER = "SPHERE_DEFINITION_ROOT"
ITEM_NAME = "SYNTHETIC_DEFINITION_ROOT"
ITEM_ID = 0x0EAA


def generate(output: Path) -> int:
    """Generate a minimal account fixture, then append only this probe."""

    # The base recipe has the generic c_MAN definition and a valid MUL set.
    # Keeping the mode-specific script and save edits here makes the rows
    # independent of the larger compatibility writer recipes.
    from .base import MODE as BASE_MODE
    from modes.compat_writer import write_mul_fixture

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    table_text = tables.read_text(encoding="ascii")
    table_text += f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME={ITEM_NAME}
NAME=synthetic definition root
TYPE=T_EQ_SCRIPT
LAYER=30
ON=@Equip
SRC.SYSMESSAGE {MARKER} C|def_dispid|[<DEF.DISPID>]
SRC.SYSMESSAGE {MARKER} C|def_name|[<?<DEF.DISPID>.NAME?>]
SRC.SYSMESSAGE {MARKER} C|def_baseid|[<?<DEF.DISPID>.BASEID?>]
SRC.SYSMESSAGE {MARKER} C|def_layer|[<?<DEF.DISPID>.LAYER?>]
SRC.SYSMESSAGE {MARKER} C|def_tdata1|[<?<DEF.DISPID>.TDATA1?>]
SRC.SYSMESSAGE {MARKER} C|def_height|[<?<DEF.DISPID>.HEIGHT?>]
SRC.SYSMESSAGE {MARKER} C|def_can|[<?<DEF.DISPID>.CAN?>]
SRC.SYSMESSAGE {MARKER} C|def_resources|[<?<DEF.DISPID>.RESOURCES?>]
SRC.SYSMESSAGE {MARKER} C|def_resource_names|[<?<DEF.DISPID>.RESOURCENAMES?>]
SRC.SYSMESSAGE {MARKER} C|cap_ascii|[<STRFIRSTCAP(hello world)>]
SRC.SYSMESSAGE {MARKER} C|cap_mixed|[<STRFIRSTCAP(hELLO wORLD)>]
SRC.SYSMESSAGE {MARKER} C|cap_multi|[<STRFIRSTCAP(two words here)>]
SRC.SYSMESSAGE {MARKER} C|cap_apostrophe|[<STRFIRSTCAP(l'etoile)>]
SRC.SYSMESSAGE {MARKER} C|cap_empty|[<STRFIRSTCAP()>]
SRC.SYSMESSAGE {MARKER} C|cap_one|[<STRFIRSTCAP(x)>]
SRC.SYSMESSAGE {MARKER} C|cap_spaces|[<STRFIRSTCAP(  hello   world)>]
SRC.SYSMESSAGE {MARKER}_END

[EVENTS {EVENT_NAME}]
ON=@LogIn
NEWITEM {ITEM_NAME}
EQUIPLAST
RETURN 0
"""
    tables.write_text(table_text, encoding="ascii")

    accounts = output / "accounts" / "sphereaccu.scp"
    accounts.write_text(
        "\n".join(
            [
                f"[ACCOUNT {ACCOUNT}]",
                f"PASSWORD={PASSWORD}",
                "CHARUID=3",
                "LASTCHARUID=3",
                "[EOF]",
            ]
        )
        + "\n",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    (output / "save" / "spherechars.scp").write_text(
        "\n".join(
            [
                'TITLE="Sphere synthetic definition-root fixture"',
                'VERSION="0.99z8"',
                "SAVECOUNT=0",
                "[WORLDCHAR c_MAN]",
                "SERIAL=3",
                f"ACCOUNT={ACCOUNT}",
                "NAME=DefinitionRootProbeCharacter",
                f"EVENTS={EVENT_NAME}",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                "P=128,128,0",
                "[EOF]",
            ]
        )
        + "\n",
        encoding="ascii",
    )
    (output / "save" / "sphereworld.scp").write_text(
        'TITLE="Sphere synthetic definition-root fixture"\nVERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    write_mul_fixture(output, extra_item_id=ITEM_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="definition-root",
        fixture_args=None,
        order=77,
        id_block=77,
        case=FixtureCase(
            name="definition-root",
            mode=None,
            tests=(TestCase("test_definition_root.py", (), True, True),),
            ports={"native": 2878, "asan": 2879},
            mode_by_variant={},
            generator="make_definition_root_fixture.py",
            generator_args=(),
            output="definition-root",
            test_args_by_variant={},
        ),
    )
)
