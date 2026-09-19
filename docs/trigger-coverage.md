# Trigger coverage

The trigger tables name script hooks, but a table entry does not by itself
mean the C++ engine fires that hook. The lists below are based on the active
`OnTrigger` and `OnTriggerScript` call sites. Commented-out rows are not
compiled table entries.

`CCharDef` trigger names use `@Name`. It also contains `@ItemName` and
`@SkillName` entries generated from the item and skill tables. `CItemDef`
trigger names use `@Name`.

## Character definitions

The engine has native call sites for:

`Create`, `Death`, `DeathCorpse`, `EnvironChange`, `GetHit`, `Hit`, `HitMiss`,
`HitTry`, `LogIn`, `LogOut`, `NPCAcceptItem`, `NPCHearGreeting`,
`NPCHearUnknown`, `NPCRefuseItem`, `NPCRestock`, `NPCSeeNewPlayer`,
`NPCSeeWantItem`, `PersonalSpace`, `ReceiveItem`, `SpellCast`, `SpellEffect`,
`Step`, `UserButton`, `UserClick`, `UserDClick`, and `UserToolTip`.

The generated character hooks `SkillAbort`, `SkillFail`, `SkillMakeItem`,
`SkillSelect`, `SkillStart`, `SkillStroke`, and `SkillSuccess` are also fired by
the skill code. The same paths offer the matching hook to the `CSkillDef`.

Currently table-only character names, reachable by `TRIGGER`, are `Destroy`,
`ItemCreate`, `ItemDestroy`, and `NPCHearNeed`, plus:

`BeforeSwing`, `AfterSwing`, `beforeGetSwing`, `afterGetSwing`,
`beforeDoEffect`, `afterDoEffect`, `beforeGetEffect`, `finalBlow`,
`DrinkingPotion`, `playerKill`, and `npckill`.

When an indexed item trigger fires through `CItem::OnTrigger` with an attached
character source, it is also offered to that character as the corresponding
`@ItemName` trigger. The item `Create` hook is called directly on its item
definition and does not pass through this reflection path.

## Item definitions

The engine has native call sites for:

`Create`, `Damage`, `Dropon_Ground`, `Equip`, `Pickup_Ground`, `Pickup_Pack`,
`SpellEffect`, `Step`, `Targon_Char`, `Targon_Ground`, `Targon_Item`, `Timer`,
`UnEquip`, `UserClick`, `UserDClick`, and `UserToolTip`.

`Destroy` is currently table-only and can be fired by `TRIGGER`. The commented
rows `Dropon_Char`, `Dropon_Item`, `EquipTest`, and `Stackon` are inactive; they
are not generated table entries.

## Custom names

Linked resources collect their `ON=` names when the section is loaded. That
list is copied when item and character definitions are lazily translated from
resource stubs. `TRIGGER @Name` can therefore find custom names even when they
are absent from the compiled tables. A custom name has no native firing site
unless C++ explicitly calls it.

The relevant dispatch paths are `CChar::OnTrigger`, `CItem::OnTrigger`,
`CItem::CreateScript`, and `CResourceTriggered::OnTriggerScript`.
