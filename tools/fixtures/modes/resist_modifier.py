"""Registered fixture for absolute resistance reductions and queued records."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .compat_writer import write_mul_fixture
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "ResistModifierProbe"
PASSWORD = "resistmodpw"
ITEM_ID = 0x0E7B
ITEM_NAME = "SYNTHETIC_RESIST_MODIFIER"
CHAR_SERIAL = 3
ITEM_SERIAL = 4
MARKER = "SPHERE_RESIST_MODIFIER"
END_MARKER = MARKER + "_END"

SCRIPT = r'''
[ITEMDEF 0x0E7B]
DEFNAME=SYNTHETIC_RESIST_MODIFIER
NAME=synthetic resistance modifier probe
TYPE=T_NORMAL
TIMER=15
ON=@Timer
FINDUID(3).F_RESIST_PROBE
RETURN 0

[FUNCTION F_RESIST_PROBE]
TAG(resist_pierce,600)
REDUCERESIST(pierce,45,200,0)
SYSMESSAGE SPHERE_RESIST_MODIFIER|single=<TAG(resist_pierce)>;timer=<FINDID(i_resistModifier_pierce).TIMERD>;records=<FINDID(i_resistModifier_pierce).TAG(records)>
REDUCERESIST(pierce,25,300,0)
SYSMESSAGE SPHERE_RESIST_MODIFIER|queue=<TAG(resist_pierce)>;timer=<FINDID(i_resistModifier_pierce).TIMERD>;records=<FINDID(i_resistModifier_pierce).TAG(records)>
REDUCERESIST(pierce,25,250,0)
REDUCERESIST(pierce,30,300,0)
REDUCERESIST(pierce,35,250,0)
SYSMESSAGE SPHERE_RESIST_MODIFIER|insert=<TAG(resist_pierce)>;timer=<FINDID(i_resistModifier_pierce).TIMERD>;records=<FINDID(i_resistModifier_pierce).TAG(records)>
FINDID(i_resistModifier_pierce).TRIGGER(TIMER)
SYSMESSAGE SPHERE_RESIST_MODIFIER|after_first_timer=<TAG(resist_pierce)>;timer=<FINDID(i_resistModifier_pierce).TIMERD>;records=<FINDID(i_resistModifier_pierce).TAG(records)>
FINDID(i_resistModifier_pierce).TRIGGER(TIMER)
SYSMESSAGE SPHERE_RESIST_MODIFIER|after_second_timer=<TAG(resist_pierce)>;timer=<FINDID(i_resistModifier_pierce).TIMERD>;records=<FINDID(i_resistModifier_pierce).TAG(records)>
FINDID(i_resistModifier_pierce).TRIGGER(TIMER)
SYSMESSAGE SPHERE_RESIST_MODIFIER|after_remove=<TAG(resist_pierce)>;memory=<FINDID(i_resistModifier_pierce)>
TAG(resist_slash,600)
REDUCEBASERESIST(slash,200)
SYSMESSAGE SPHERE_RESIST_MODIFIER|base=<TAG(resist_slash)>
FINDID(i_resistModifier_slash).REMOVE
SYSMESSAGE SPHERE_RESIST_MODIFIER_END
RETURN 0

[FUNCTION REDUCERESIST]
ARG(mem_resist,<F_RESISTMODIFIER_GETMEMORY(<ARGV(0)>)>)
MEM_RESIST.F_RESISTMODIFIER_ADDRECORD(<ARGV(1)>,<ARGV(2)>,<ARGV(3)>)

[FUNCTION REDUCEBASERESIST]
ARG(mem_resist,<F_RESISTMODIFIER_GETMEMORY(<ARGV(0)>)>)
MEM_RESIST.MORE1=<MEM_RESIST.MORE1>-<ARGV(1)>
MEM_RESIST.F_RESISTMODIFIER_APPLYCURRENT

[FUNCTION F_RESISTMODIFIER_GETMEMORY]
ARG(memId,<FINDID(i_resistModifier_<ARGV(0)>)>)
IF (STRLEN(<ARG(memId)>))
RETURN <ARG(memId)>
ENDIF
NEWEQUIPSAFE(i_resistModifier_<ARGV(0)>)
LASTNEW.MORE1=<EVAL TAG(resist_<ARGV(0)>)>
LASTNEW.MORE2=<EVAL TAG(resist_<ARGV(0)>)>
LASTNEW.TAG(resistType,<ARGV(0)>)
LASTNEW.TAG(records,0)
RETURN <LASTNEW>

[FUNCTION NEWEQUIPSAFE]
IF (STRLEN("<ARGS>"))
  IF (ISITEM)
    RETURN 0
  ELSE
    NEWEquip(<HVAL ARGV(0)>)
  ENDIF
ENDIF
RETURN 0

[FUNCTION F_RESISTMODIFIER_ADDRECORD]
ARG(endTime,<SERV.TIME>+<ARGV(0)>)
IF (0<ARGV(2)>)
  IF (<MORE1> > 0)
    ARG(value,<EVAL (<MORE1>*<ARGV(1)>)/100>)
  ELSE
    IF (0<TAG(records)> == 0)
      REMOVE
    ENDIF
    RETURN
  ENDIF
ELSE
  ARG(value,<ARGV(1)>)
ENDIF
ARG(records,<EVAL TAG(records)>)
IF (<F_RESISTMODIFIER_FINDPLACEMENT(<ARG(endTime)>,<ARG(value)>,<ARG(records)>)>)
  RETURN
ENDIF
ARG(i,<ARG(records)>)
WHILE (ARG(i) > <VAR(PLACEMENT)>)
  TAG(record[<EVAL ARG(i)>],"<TAG(record[<EVAL (ARG(i)-1)>])>")
  ARG(i,#-1)
ENDWHILE
TAG(record[<EVAL VAR(PLACEMENT)>],"<ARG(endTime)>,<ARG(value)>")
TAG(records,#+1)
IF (<VAR(PLACEMENT)> == 0)
  F_RESISTMODIFIER_APPLYCURRENT
ENDIF

[FUNCTION F_RESISTMODIFIER_FINDPLACEMENT]
VAR(placement,0)
WHILE (VAR(placement) < ARGV(2))
  ARG(cur_endTime,<RETURN_NTH(1,<TAG(record[<EVAL VAR(PLACEMENT)>])>)>)
  ARG(cur_value,<RETURN_NTH(2,<TAG(record[<EVAL VAR(PLACEMENT)>])>)>)
  IF (ARG(cur_value) <= ARGV(1))
    IF (ARG(cur_endTime) < ARGV(0))
      TAG(record[<EVAL VAR(PLACEMENT)>],"<ARGV(0)>,<ARGV(1)>")
      F_RESISTMODIFIER_CLEANINVALID(<EVAL VAR(PLACEMENT)+1>,<ARGV(2)>,<ARGV(0)>)
      IF (VAR(PLACEMENT) == 0)
        F_RESISTMODIFIER_APPLYCURRENT
      ENDIF
      RETURN 1
    ELSEIF (<ARG(cur_value)>==<ARGV(1)>)
      RETURN 1
    ENDIF
    RETURN 0
  ELSEIF (ARG(cur_endTime) >= <ARGV(0)>)
    RETURN 1
  ENDIF
  VAR(PLACEMENT,#+1)
ENDWHILE
RETURN 0

[FUNCTION F_RESISTMODIFIER_CLEANINVALID]
ARG(i,<ARGV(0)>)
ARG(removed,0)
WHILE (I < <ARGV(1)>)
  ARG(cur_endTime,<RETURN_NTH(1,<TAG(record[<EVAL ARG(i)>])>)>)
  IF (cur_endTime < <ARGV(2)>)
    TAG.REMOVE(record[<EVAL ARG(i)>])
    ARG(removed,#+1)
  ELSEIF (!<ARG(removed)>)
    RETURN
  ELSE
    TAG(record[<EVAL (ARG(i)-ARG(removed))>] ,"<TAG(record[<EVAL ARG(i)>])>")
    TAG.REMOVE(record[<EVAL ARG(i)>])
  ENDIF
  ARG(i,#+1)
ENDWHILE
TAG(records,#-<ARG(removed)>)

[FUNCTION F_RESISTMODIFIER_APPLYCURRENT]
IF (STRLEN(<TAG(record_0)>))
  CONT.TAG(resist_<TAG(resistType)>,<EVAL (<MORE1> - <RETURN_NTH(2,<TAG(record_0)>)>)>)
  TIMERD=<EVAL (<RETURN_NTH(1,<TAG(record_0)>)>-<SERV.TIME>)>
ELSE
  CONT.TAG(resist_<TAG(resistType)>,<EVAL MORE1>)
ENDIF

[ITEMDEF i_resistModifier_pierce]
ID=0x0E75
NAME=synthetic resist modifier
TYPE=T_EQ_SCRIPT
ON=@Timer
F_RESISTMODIFIER_TIMER
ON=@Unequip
F_RESISTMODIFIER_UNEQUIP

[ITEMDEF i_resistModifier_slash]
ID=0x0E76
NAME=synthetic slash resist modifier
TYPE=T_EQ_SCRIPT
ON=@Timer
F_RESISTMODIFIER_TIMER
ON=@Unequip
F_RESISTMODIFIER_UNEQUIP

[FUNCTION F_RESISTMODIFIER_TIMER]
IF (STRLEN(<TAG(record_0)>))
  TAG.REMOVE(record_0)
  IF (TAG(records) > 1)
    TAG(record_0,"<TAG(record_1)>")
    TAG.REMOVE(record_1)
    TAG(records,#-1)
    F_RESISTMODIFIER_APPLYCURRENT
  ELSE
    REMOVE
  ENDIF
ELSE
  REMOVE
ENDIF

[FUNCTION F_RESISTMODIFIER_UNEQUIP]
TAG.REMOVE(records)
CONT.TAG(resist_<TAG(resistType)>,<EVAL MORE2>)

[FUNCTION RETURN_NTH]
RETURN<ARGV(<ARGV(0)>)>
'''


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result
    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(tables.read_text(encoding="ascii") + "\n" + SCRIPT, encoding="ascii")
    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]\nPASSWORD={PASSWORD}\nCHARUID={CHAR_SERIAL}\nLASTCHARUID={CHAR_SERIAL}\n[EOF]\n""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    (output / "save" / "sphereworld.scp").write_text(
        f"""TITLE=\"Sphere synthetic resistance modifier fixture\"\nVERSION=\"0.99z8\"\nSAVECOUNT=0\n[WorldItem {ITEM_NAME}]\nSERIAL={ITEM_SERIAL}\nP=128,128,0\nTIMER=15\n[EOF]\n""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=\"Sphere synthetic resistance modifier fixture\"\nVERSION=\"0.99z8\"\nSAVECOUNT=0\n[WORLDCHAR c_MAN]\nSERIAL={CHAR_SERIAL}\nNAME=ResistModifierProbeCharacter\nACCOUNT={ACCOUNT}\nSTR=100\nINT=100\nDEX=100\nHITS=100\nMAXHITS=100\nMANA=100\nMAXMANA=100\nSTAM=100\nMAXSTAM=100\nP=128,128,0\n[EOF]\n""",
        encoding="ascii",
    )
    write_mul_fixture(output, extra_item_id=ITEM_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="resist-modifier",
        fixture_args=(),
        order=217,
        id_block=163,
        case=FixtureCase(
            name="resist-modifier",
            mode="resist-modifier",
            tests=(TestCase("test_resist_modifier.py", (), True, True),),
            ports={"native": 45220, "asan": 45221},
            output="resist-modifier",
        ),
    )
)
