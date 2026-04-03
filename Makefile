CXX = g++
BASEDIR = $(CURDIR)
CXXFLAGS = -g -std=c++14 -m32 -fpermissive -Wno-endif-labels -Wno-write-strings \
           -Wno-narrowing -Wno-unused-result -Wno-format-security \
           -DSPHERE_SVR -D_CONSOLE -D_MT \
           -I$(BASEDIR) -I$(BASEDIR)/spherelib -I$(BASEDIR)/SphereCommon \
           -I$(BASEDIR)/SphereAccount -I$(BASEDIR)/SphereSvr
LDFLAGS = -m32 -lpthread
TARGET = sphere99svr

# Source files - excluding Windows-only files
SPHERELIB_SRC = \
	spherelib/CPointBase.cpp \
	spherelib/CScript.cpp \
	spherelib/cfile.cpp \
	spherelib/stubs.cpp \
	spherelib/cstring.cpp \
	spherelib/ctime.cpp

SPHERECOMMON_SRC = \
	SphereCommon/stubs.cpp \
	SphereCommon/ccrypt.cpp \
	SphereCommon/ccryptnew.cpp \
	SphereCommon/cMulInst.cpp \
	SphereCommon/cmulmap.cpp \
	SphereCommon/cmulmulti.cpp \
	SphereCommon/cmultile.cpp \
	SphereCommon/cmulver.cpp \
	SphereCommon/cobjbasetemplate.cpp \
	SphereCommon/cpointmap.cpp \
	SphereCommon/cregioncomplex.cpp \
	SphereCommon/cregionmap.cpp \
	SphereCommon/cregiontype.cpp \
	SphereCommon/cresourcebase.cpp \
	SphereCommon/csectortemplate.cpp \
	SphereCommon/cSphereExp.cpp \
	SphereCommon/cteleport.cpp \
	SphereCommon/spherepatch.cpp \
	SphereCommon/sphereproto.cpp

SPHEREACCOUNT_SRC = \
	SphereAccount/CAccount.cpp \
	SphereAccount/caccountmgr.cpp

SPHERESVR_SRC = \
	SphereSvr/cbacktask.cpp \
	SphereSvr/ccharact.cpp \
	SphereSvr/CChar.cpp \
	SphereSvr/cCharDef.cpp \
	SphereSvr/ccharfight.cpp \
	SphereSvr/ccharnoto.cpp \
	SphereSvr/ccharnpcact.cpp \
	SphereSvr/CCharNPC.cpp \
	SphereSvr/ccharnpcfood.cpp \
	SphereSvr/ccharnpcpet.cpp \
	SphereSvr/ccharnpcstatus.cpp \
	SphereSvr/ccharskill.cpp \
	SphereSvr/ccharspell.cpp \
	SphereSvr/ccharstatus.cpp \
	SphereSvr/ccharuse.cpp \
	SphereSvr/CChat.cpp \
	SphereSvr/CClient.cpp \
	SphereSvr/cclientdialog.cpp \
	SphereSvr/cclientevent.cpp \
	SphereSvr/cclientgmpage.cpp \
	SphereSvr/cclientlog.cpp \
	SphereSvr/cclientmsg.cpp \
	SphereSvr/cclienttarg.cpp \
	SphereSvr/cclientuse.cpp \
	SphereSvr/CContain.cpp \
	SphereSvr/cgmpage.cpp \
	SphereSvr/citemcont.cpp \
	SphereSvr/CItem.cpp \
	SphereSvr/cItemDef.cpp \
	SphereSvr/citemmulti.cpp \
	SphereSvr/CItemSp.cpp \
	SphereSvr/citemstone.cpp \
	SphereSvr/citemvend.cpp \
	SphereSvr/CLog.cpp \
	SphereSvr/CObjBase.cpp \
	SphereSvr/cobjbasedef.cpp \
	SphereSvr/cquest.cpp \
	SphereSvr/cresourcecalc.cpp \
	SphereSvr/cresource.cpp \
	SphereSvr/cresourcedef.cpp \
	SphereSvr/cresourcetest.cpp \
	SphereSvr/csector.cpp \
	SphereSvr/cservconsoled.cpp \
	SphereSvr/CServer.cpp \
	SphereSvr/CServRef.cpp \
	SphereSvr/CServResource.cpp \
	SphereSvr/CWebPage.cpp \
	SphereSvr/CWorld.cpp \
	SphereSvr/cworldimport.cpp \
	SphereSvr/cworldmap.cpp \
	SphereSvr/CWorldSearch.cpp \
	SphereSvr/spheresvr.cpp \
	SphereSvr/stubs.cpp

# Excluded: cservconsolew.cpp (Windows GUI), cDSound.cpp/cdsoundchat.cpp (DirectSound),
#           StdAfx.cpp files (empty precompiled header stubs)

ALL_SRC = $(SPHERELIB_SRC) $(SPHERECOMMON_SRC) $(SPHEREACCOUNT_SRC) $(SPHERESVR_SRC)
ALL_OBJ = $(ALL_SRC:.cpp=.o)

all: $(TARGET)

$(TARGET): $(ALL_OBJ)
	$(CXX) $(ALL_OBJ) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(ALL_OBJ) $(TARGET)

.PHONY: all clean
