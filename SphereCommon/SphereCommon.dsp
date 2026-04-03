# Microsoft Developer Studio Project File - Name="SphereCommon" - Package Owner=<4>

# Microsoft Developer Studio Generated Build File, Format Version 6.00

# ** DO NOT EDIT **



# TARGTYPE "Win32 (x86) Static Library" 0x0104



CFG=SphereCommon - Win32 Debug

!MESSAGE This is not a valid makefile. To build this project using NMAKE,

!MESSAGE use the Export Makefile command and run

!MESSAGE 

!MESSAGE NMAKE /f "SphereCommon.mak".

!MESSAGE 

!MESSAGE You can specify a configuration when running NMAKE

!MESSAGE by defining the macro CFG on the command line. For example:

!MESSAGE 

!MESSAGE NMAKE /f "SphereCommon.mak" CFG="SphereCommon - Win32 Debug"

!MESSAGE 

!MESSAGE Possible choices for configuration are:

!MESSAGE 

!MESSAGE "SphereCommon - Win32 Release" (based on "Win32 (x86) Static Library")

!MESSAGE "SphereCommon - Win32 Debug" (based on "Win32 (x86) Static Library")

!MESSAGE 



# Begin Project

# PROP AllowPerConfigDependencies 0

# PROP Scc_ProjName ""$/SphereCommon", NPBAAAAA"

# PROP Scc_LocalPath "."

CPP=cl.exe

RSC=rc.exe



!IF  "$(CFG)" == "SphereCommon - Win32 Release"



# PROP BASE Use_MFC 0

# PROP BASE Use_Debug_Libraries 0

# PROP BASE Output_Dir "Release"

# PROP BASE Intermediate_Dir "Release"

# PROP BASE Target_Dir ""

# PROP Use_MFC 0

# PROP Use_Debug_Libraries 0

# PROP Output_Dir "Release"

# PROP Intermediate_Dir "Release"

# PROP Target_Dir ""

# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /Yu"stdafx.h" /FD /c

# ADD CPP /nologo /Gz /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /Yu"stdafx.h" /FD /c

# ADD BASE RSC /l 0x409 /d "NDEBUG"

# ADD RSC /l 0x409 /d "NDEBUG"

BSC32=bscmake.exe

# ADD BASE BSC32 /nologo

# ADD BSC32 /nologo

LIB32=link.exe -lib

# ADD BASE LIB32 /nologo

# ADD LIB32 /nologo



!ELSEIF  "$(CFG)" == "SphereCommon - Win32 Debug"



# PROP BASE Use_MFC 0

# PROP BASE Use_Debug_Libraries 1

# PROP BASE Output_Dir "Debug"

# PROP BASE Intermediate_Dir "Debug"

# PROP BASE Target_Dir ""

# PROP Use_MFC 0

# PROP Use_Debug_Libraries 1

# PROP Output_Dir "Debug"

# PROP Intermediate_Dir "Debug"

# PROP Target_Dir ""

# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /Yu"stdafx.h" /FD /GZ /c

# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /Yu"stdafx.h" /FD /GZ /c

# ADD BASE RSC /l 0x409 /d "_DEBUG"

# ADD RSC /l 0x409 /d "_DEBUG"

BSC32=bscmake.exe

# ADD BASE BSC32 /nologo

# ADD BSC32 /nologo

LIB32=link.exe -lib

# ADD BASE LIB32 /nologo

# ADD LIB32 /nologo



!ENDIF 



# Begin Target



# Name "SphereCommon - Win32 Release"

# Name "SphereCommon - Win32 Debug"

# Begin Group "Source Files"



# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"

# Begin Source File



SOURCE=.\ccrypt.cpp

# End Source File

# Begin Source File



SOURCE=.\ccryptnew.cpp

# End Source File

# Begin Source File



SOURCE=.\cDSound.cpp

# End Source File

# Begin Source File



SOURCE=.\cDSoundChat.cpp

# End Source File

# Begin Source File



SOURCE=.\cMulInst.cpp

# End Source File

# Begin Source File



SOURCE=.\cMulMap.cpp

# End Source File

# Begin Source File



SOURCE=.\cmulmulti.cpp

# End Source File

# Begin Source File



SOURCE=.\cMulTile.cpp

# End Source File

# Begin Source File



SOURCE=.\cMulVer.cpp

# End Source File

# Begin Source File



SOURCE=.\cObjBaseTemplate.cpp

# End Source File

# Begin Source File



SOURCE=.\cPointMap.cpp

# End Source File

# Begin Source File



SOURCE=.\CRegionMap.cpp

# End Source File

# Begin Source File



SOURCE=.\cresourcebase.cpp

# End Source File

# Begin Source File



SOURCE=.\csectortemplate.cpp

# End Source File

# Begin Source File



SOURCE=.\spherepatch.cpp

# End Source File

# Begin Source File



SOURCE=.\sphereproto.cpp

# End Source File

# Begin Source File



SOURCE=.\StdAfx.cpp

# ADD CPP /Yc"stdafx.h"

# End Source File

# End Group

# Begin Group "Header Files"



# PROP Default_Filter "h;hpp;hxx;hm;inl"

# Begin Source File



SOURCE=.\ccrypt.h

# End Source File

# Begin Source File



SOURCE=.\cdsound.h

# End Source File

# Begin Source File



SOURCE=.\cMulInst.h

# End Source File

# Begin Source File



SOURCE=.\cMulMap.h

# End Source File

# Begin Source File



SOURCE=.\CMulMulti.h

# End Source File

# Begin Source File



SOURCE=.\cMulTile.h

# End Source File

# Begin Source File



SOURCE=.\cObjBaseTemplate.h

# End Source File

# Begin Source File



SOURCE=.\cPointMap.h

# End Source File

# Begin Source File



SOURCE=.\CRegionMap.h

# End Source File

# Begin Source File



SOURCE=.\cresourcebase.h

# End Source File

# Begin Source File



SOURCE=.\cSectorTemplate.h

# End Source File

# Begin Source File



SOURCE=.\SphereCommon.h

# End Source File

# Begin Source File



SOURCE=.\spheremul.h

# End Source File

# Begin Source File



SOURCE=.\spherepatch.h

# End Source File

# Begin Source File



SOURCE=.\sphereproto.h

# End Source File

# Begin Source File



SOURCE=.\StdAfx.h

# End Source File

# End Group

# End Target

# End Project

