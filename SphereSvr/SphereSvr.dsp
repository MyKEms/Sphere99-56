# Microsoft Developer Studio Project File - Name="SphereSvr" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=SphereSvr - Win32 DebugProfile
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SphereSvr.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SphereSvr.mak" CFG="SphereSvr - Win32 DebugProfile"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SphereSvr - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "SphereSvr - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "SphereSvr - Win32 DebugProfile" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/SphereSvr", FCAAAAAA"
# PROP Scc_LocalPath "d:\menace\spheresvr"
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SphereSvr - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /Gz /MT /W2 /GR /GX /O1 /Ob2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "SPHERE_SVR" /FR /Yu"stdafx.h" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 wsock32.lib kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib comctl32.lib D:\samples\js\bin\js32.lib /nologo /version:0.10 /subsystem:windows /map /debug /machine:I386
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "SphereSvr - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /Gz /MTd /W2 /GR /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "SPHERE_SVR" /FR /Yu"stdafx.h" /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 wsock32.lib kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib comctl32.lib D:\samples\js\bin\js32.lib /nologo /version:0.12 /subsystem:windows /map /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "SphereSvr - Win32 DebugProfile"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SphereSvr___Win32_DebugProfile"
# PROP BASE Intermediate_Dir "SphereSvr___Win32_DebugProfile"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "DebugProfile"
# PROP Intermediate_Dir "DebugProfile"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /Gz /Zp4 /MTd /W2 /GR /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "SPHERE_SVR" /FR /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /Gz /Zp4 /MTd /W2 /GR /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "SPHERE_SVR" /D "USE_JSCRIPT" /FR /Yu"stdafx.h" /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 wsock32.lib kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib /nologo /version:0.12 /subsystem:windows /map /debug /machine:I386 /out:"Debug/sphereSvr.exe" /pdbtype:sept
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 wsock32.lib kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib comctl32.lib D:\samples\js\bin\js32.lib /nologo /version:0.12 /subsystem:windows /profile /map /debug /machine:I386 /out:"Debug/sphereSvr.exe"

!ENDIF 

# Begin Target

# Name "SphereSvr - Win32 Release"
# Name "SphereSvr - Win32 Debug"
# Name "SphereSvr - Win32 DebugProfile"
# Begin Group "Scripts"

# PROP Default_Filter "scp,ini"
# Begin Group "Scripts bs3"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\scripts\bs3\bs2_crier.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs2_items.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs2_map.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs2_santa.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs2_stuff.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs3_borgvendors.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs3_classitems.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs3_drowvendors.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs3_dwarfvendors.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs3_elfvendors.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs3_gargoyle_town.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs3_halforcvendors.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs3_helpgump.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs3_ogre_town.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs3_ore_elem.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs3_patron.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs3_racemenu.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\bs3_troll_town.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\sphereclass.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\bs3\SphereRace.scp
# End Source File
# End Group
# Begin Group "Scripts Speech"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\scripts\speech\jobactor.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobalchemist.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobanimal.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobarchitect.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobarmourer.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobartist.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobbaker.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobbanker.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobbard.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobbeekeeper.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobbeggar.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobblacksmith.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobbowyer.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobBrigand.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobbutcher.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobcarpenter.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobcashual.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobcobbler.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobfarmer.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobfurtrader.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobgambler.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobglassblower.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobGuard.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobhealer.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobhealwand.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobherbalist.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobinnkeeper.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobjailor.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobjeweller.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobjudge.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\joblaborer.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobMage.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobMageEvil.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobMageShop.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobmapmaker.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobmayor.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobmiller.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobminer.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobminter.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobmonk.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobnoble.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\joboverseer.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobpaladin.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobparliament.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobpirate.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobpriest.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobprisoner.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobprovisioner.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobrancher.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobranger.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobrealtor.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobrunner.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobsailor.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobscholar.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobscribe.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobsculptor.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobservant.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobshepherd.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobshipwright.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobtanner.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobtavernkeeper.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobthief.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobtinker.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobveggie.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobvet.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobwaiter.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobweaponsmith.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobweaponstrainer.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\jobweaver.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\speakhorse.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\speakhuman.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\speakmaster.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\speakneeds.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\speakorc.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\speakrehello.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\speakshopkeep.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\spherespee.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\townbritain.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\townbucsden.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\towncove.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\townjhelom.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\townmagincia.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\townminoc.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\townmoonglow.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\townnujelm.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\townserphold.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\townskarabrae.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\towntrinsic.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\townvesper.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\townwind.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\speech\townyew.scp
# End Source File
# End Group
# Begin Group "Scripts Test"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\scripts\test\blacksmith_menu.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\test\classmenus.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\test\clericbook.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\test\custom_vendor.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\test\helpgump2.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\test\helpgump3.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\test\housegump.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\test\housegump2.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\test\housegump3.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\test\necrobook.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\test\RewardSystem.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\test\runebook.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\test\runebook2.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\test\spherechar3.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\custom\spheretest.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\test\Tinkering_menu.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\test\traveler.scp
# End Source File
# End Group
# Begin Group "NPCs"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char3.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_animals.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_borgvendors.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_crier.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_daemon.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_dragon.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_drowvendors.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_dwarfvendors.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_elfvendors.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_evil.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_gargvendors.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_halforcvendors.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_humans.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_lbr_new.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_monsters.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_mustang.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_ogrevendors.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_ophidian.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_skill_testers.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_traveller.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_trollvendors.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_undead.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\npcs\sphere_d_char_uniques.scp
# End Source File
# End Group
# Begin Source File

SOURCE=.\sphere.ini
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_book.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_defs.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_dialog.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_dlg_reference.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_events.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_events_human.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_function.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_healing.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_house_item.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_house_menu.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_map.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_map2.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_menu.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_name.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_newb.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_plevel.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_region.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_skillmenu.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_spawns.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_speech.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_spells.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_template.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_template_vend.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_templates_loot.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_trig.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\sphere_d_web.scp
# End Source File
# Begin Source File

SOURCE=..\scripts\SPHERETables.scp
# End Source File
# End Group
# Begin Group "Sources"

# PROP Default_Filter "cpp,c,h,rc"
# Begin Source File

SOURCE=.\CBackTask.cpp
# End Source File
# Begin Source File

SOURCE=.\CChar.cpp
# End Source File
# Begin Source File

SOURCE=.\ccharact.cpp
# End Source File
# Begin Source File

SOURCE=.\cCharDef.cpp
# End Source File
# Begin Source File

SOURCE=.\CCharFight.cpp
# End Source File
# Begin Source File

SOURCE=.\CCharNoto.cpp
# End Source File
# Begin Source File

SOURCE=.\CCharNPC.cpp
# End Source File
# Begin Source File

SOURCE=.\CCharNPCAct.cpp
# End Source File
# Begin Source File

SOURCE=.\CCharNPCFood.cpp
# End Source File
# Begin Source File

SOURCE=.\CCharNPCPet.cpp
# End Source File
# Begin Source File

SOURCE=.\CCharNPCStatus.cpp
# End Source File
# Begin Source File

SOURCE=.\ccharskill.cpp
# End Source File
# Begin Source File

SOURCE=.\ccharspell.cpp
# End Source File
# Begin Source File

SOURCE=.\CCharStatus.cpp
# End Source File
# Begin Source File

SOURCE=.\ccharuse.cpp
# End Source File
# Begin Source File

SOURCE=.\CChat.cpp
# End Source File
# Begin Source File

SOURCE=.\CClient.cpp
# End Source File
# Begin Source File

SOURCE=.\CClientDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\cclientevent.cpp
# End Source File
# Begin Source File

SOURCE=.\CClientGMPage.cpp
# End Source File
# Begin Source File

SOURCE=.\CClientLog.cpp
# End Source File
# Begin Source File

SOURCE=.\CClientMsg.cpp
# End Source File
# Begin Source File

SOURCE=.\cclienttarg.cpp
# End Source File
# Begin Source File

SOURCE=.\cclientuse.cpp
# End Source File
# Begin Source File

SOURCE=.\CContain.cpp
# End Source File
# Begin Source File

SOURCE=.\cgmpage.cpp
# End Source File
# Begin Source File

SOURCE=.\CItem.cpp
# End Source File
# Begin Source File

SOURCE=.\CItemCont.cpp
# End Source File
# Begin Source File

SOURCE=.\cItemDef.cpp
# End Source File
# Begin Source File

SOURCE=.\citemmulti.cpp
# End Source File
# Begin Source File

SOURCE=.\CItemSp.cpp
# End Source File
# Begin Source File

SOURCE=.\CItemStone.cpp
# End Source File
# Begin Source File

SOURCE=.\CItemVend.cpp
# End Source File
# Begin Source File

SOURCE=.\CLog.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjBase.cpp
# End Source File
# Begin Source File

SOURCE=.\cObjBaseDef.cpp
# End Source File
# Begin Source File

SOURCE=.\cquest.cpp
# End Source File
# Begin Source File

SOURCE=.\csector.cpp
# End Source File
# Begin Source File

SOURCE=.\CServConsoleD.cpp
# End Source File
# Begin Source File

SOURCE=.\CServConsoleW.cpp
# End Source File
# Begin Source File

SOURCE=.\CServer.cpp
# End Source File
# Begin Source File

SOURCE=.\CServRef.cpp
# End Source File
# Begin Source File

SOURCE=.\CWebPage.cpp
# End Source File
# Begin Source File

SOURCE=.\CWorld.cpp
# End Source File
# Begin Source File

SOURCE=.\cworldimport.cpp
# End Source File
# Begin Source File

SOURCE=.\cworldmap.cpp
# End Source File
# Begin Source File

SOURCE=.\CWorldSearch.cpp
# End Source File
# Begin Source File

SOURCE=.\SpherePublic.h
# End Source File
# Begin Source File

SOURCE=.\spheresvr.cpp
# End Source File
# Begin Source File

SOURCE=.\spheresvr.rc
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\CChat.h
# End Source File
# Begin Source File

SOURCE=.\cclient.h
# End Source File
# Begin Source File

SOURCE=.\cobjbase.h
# End Source File
# Begin Source File

SOURCE=.\cObjBaseDef.h
# End Source File
# Begin Source File

SOURCE=.\CServRef.h
# End Source File
# Begin Source File

SOURCE=.\CWorld.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\spheresvr.h
# End Source File
# Begin Source File

SOURCE=.\stdafx.h
# End Source File
# End Group
# Begin Group "SphereCommon"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\SphereCommon\ccrypt.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\ccrypt.h
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\ccryptnew.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cMulInst.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cMulInst.h
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cMulMap.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cMulMap.h
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cmulmulti.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\CMulMulti.h
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cMulTile.h
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cMulVer.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cObjBaseTemplate.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cObjBaseTemplate.h
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cpointmap.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cPointMap.h
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\CRegionComplex.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cregioncomplexmethods.tbl
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cregioncomplexprops.tbl
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cregionevents.tbl
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\CRegionMap.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\CRegionMap.h
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cregionmethods.tbl
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cregionprops.tbl
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cregionresourceprops.tbl
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\CRegionType.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cregiontypeprops.tbl
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cresourcebase.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cresourcebase.h
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\csectortemplate.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cSectorTemplate.h
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cSphereExp.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cSphereExp.h
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\csphereexpfunc.tbl
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\Cteleport.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\globalmethods.tbl
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\SphereCommon.h
# End Source File
# End Group
# Begin Group "SphereAccount"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\SphereAccount\CAccount.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereAccount\caccount.h
# End Source File
# Begin Source File

SOURCE=..\SphereAccount\caccountbase.h
# End Source File
# Begin Source File

SOURCE=..\SphereAccount\caccountmethods.tbl
# End Source File
# Begin Source File

SOURCE=..\SphereAccount\caccountmgr.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereAccount\caccountmgrmethods.tbl
# End Source File
# Begin Source File

SOURCE=..\SphereAccount\caccountprops.tbl
# End Source File
# End Group
# Begin Group "SphereProto"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\SphereLib\spherelib.h
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\spheremul.h
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\sphereproto.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\sphereproto.h
# End Source File
# End Group
# Begin Group "SphereResource"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\CResource.cpp
# End Source File
# Begin Source File

SOURCE=.\cresource.h
# End Source File
# Begin Source File

SOURCE=.\CResourceCalc.cpp
# End Source File
# Begin Source File

SOURCE=.\CResourceDef.cpp
# End Source File
# Begin Source File

SOURCE=.\cresourceprops.tbl
# End Source File
# Begin Source File

SOURCE=..\SphereCommon\cresourcetag.tbl
# End Source File
# Begin Source File

SOURCE=.\CResourceTest.cpp
# End Source File
# End Group
# Begin Group "Web Pages"

# PROP Default_Filter "htt"
# Begin Source File

SOURCE=..\scripts\web\index.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\sphere404.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\sphereaccount.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\sphereconfig.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\sphereconfiggame.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\sphereconfigserv.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\spherejoin.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\spherelogfail.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\spherelogin.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\spheremailpass.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\spheremsgres.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\spheremyaccount.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\sphereplayer.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\sphereplayer2.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\spheres1t.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\spheres2t.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\spheres3t.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\spheres4t.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\spheres5t.htt
# End Source File
# Begin Source File

SOURCE=..\scripts\web\spherestatus.htt
# End Source File
# End Group
# Begin Group "IRC Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\SphereIRC\CIRCServConf.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\CIRCServer.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\CIRCServer.h
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\CIRCServer2.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\hash.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\hash.h
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCBase64.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCBase64.h
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCChanServ.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCCloak.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCCommands.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCCommonServ.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCConfig.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCCRule.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCCRule.h
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCHelpServ.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCMemoServ.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCModes.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCModes.h
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCNickServ.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCOperServ.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCPacket.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCProcess.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCRootServ.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCSendTo.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCSendTo.h
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCServices.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCServices.h
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCTools.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCTools.h
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCWhoWas.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\IRCWhoWas.h
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\SCache.cpp
# End Source File
# Begin Source File

SOURCE=..\SphereIRC\SCache.h
# End Source File
# End Group
# Begin Group "Scripting Tables"

# PROP Default_Filter "tbl"
# Begin Source File

SOURCE=.\cchardefprops.tbl
# End Source File
# Begin Source File

SOURCE=.\ccharevents.tbl
# End Source File
# Begin Source File

SOURCE=.\ccharmethods.tbl
# End Source File
# Begin Source File

SOURCE=.\ccharnpcmethods.tbl
# End Source File
# Begin Source File

SOURCE=.\ccharnpcprops.tbl
# End Source File
# Begin Source File

SOURCE=.\ccharplayermethods.tbl
# End Source File
# Begin Source File

SOURCE=.\ccharplayerprops.tbl
# End Source File
# Begin Source File

SOURCE=.\ccharprops.tbl
# End Source File
# Begin Source File

SOURCE=.\cclientmethods.tbl
# End Source File
# Begin Source File

SOURCE=.\cclientprops.tbl
# End Source File
# Begin Source File

SOURCE=.\ccontainermethods.tbl
# End Source File
# Begin Source File

SOURCE=.\cgmpageprops.tbl
# End Source File
# Begin Source File

SOURCE=.\citemcontainermethods.tbl
# End Source File
# Begin Source File

SOURCE=.\citemdefmultiprops.tbl
# End Source File
# Begin Source File

SOURCE=.\citemdefprops.tbl
# End Source File
# Begin Source File

SOURCE=.\CItemDefWeaponProps.tbl
# End Source File
# Begin Source File

SOURCE=.\citemevents.tbl
# End Source File
# Begin Source File

SOURCE=.\citemmapmethods.tbl
# End Source File
# Begin Source File

SOURCE=.\citemmessagemethods.tbl
# End Source File
# Begin Source File

SOURCE=.\citemmessageprops.tbl
# End Source File
# Begin Source File

SOURCE=.\citemmethods.tbl
# End Source File
# Begin Source File

SOURCE=.\citemmultimethods.tbl
# End Source File
# Begin Source File

SOURCE=.\citemmultiprops.tbl
# End Source File
# Begin Source File

SOURCE=.\citemprops.tbl
# End Source File
# Begin Source File

SOURCE=.\citemscriptmethods.tbl
# End Source File
# Begin Source File

SOURCE=.\citemstonemethods.tbl
# End Source File
# Begin Source File

SOURCE=.\citemstoneprops.tbl
# End Source File
# Begin Source File

SOURCE=.\citemvendprops.tbl
# End Source File
# Begin Source File

SOURCE=.\cobjbasedefprops.tbl
# End Source File
# Begin Source File

SOURCE=.\cobjbasemethods.tbl
# End Source File
# Begin Source File

SOURCE=.\cobjbaseprops.tbl
# End Source File
# Begin Source File

SOURCE=.\cprofessionprops.tbl
# End Source File
# Begin Source File

SOURCE=.\cprofileprops.tbl
# End Source File
# Begin Source File

SOURCE=.\CRaceClassMethods.tbl
# End Source File
# Begin Source File

SOURCE=.\craceclassprops.tbl
# End Source File
# Begin Source File

SOURCE=.\csectormethods.tbl
# End Source File
# Begin Source File

SOURCE=.\csectorprops.tbl
# End Source File
# Begin Source File

SOURCE=.\cserverdefprops.tbl
# End Source File
# Begin Source File

SOURCE=.\cservermethods.tbl
# End Source File
# Begin Source File

SOURCE=.\cservevents.tbl
# End Source File
# Begin Source File

SOURCE=.\cskilldefevents.tbl
# End Source File
# Begin Source File

SOURCE=.\cskilldefprops.tbl
# End Source File
# Begin Source File

SOURCE=.\cspelldefprops.tbl
# End Source File
# Begin Source File

SOURCE=.\cwebpageevents.tbl
# End Source File
# Begin Source File

SOURCE=.\cwebpagemethods.tbl
# End Source File
# Begin Source File

SOURCE=.\cwebpageprops.tbl
# End Source File
# Begin Source File

SOURCE=.\cworldprops.tbl
# End Source File
# End Group
# Begin Source File

SOURCE=.\spheresvr.ico
# End Source File
# Begin Source File

SOURCE=..\SphereDoc\SphereSvr_Bugs.txt
# End Source File
# Begin Source File

SOURCE=..\SphereDoc\SphereSvr_Requests.txt
# End Source File
# Begin Source File

SOURCE=..\SphereDoc\SphereSvr_REVISIONS.txt
# End Source File
# End Target
# End Project
