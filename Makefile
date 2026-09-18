CXX ?= g++
BASEDIR := $(CURDIR)

# Keep the historical default target as a 32-bit i386 binary.  Debug and
# sanitizer targets below deliberately omit -m32 and use their own object
# directories, because ASan/gdb are not reliable under i386 qemu emulation.
COMMON_CXXFLAGS = -std=c++14 -fpermissive -Wno-endif-labels -Wno-write-strings \
                  -Wno-narrowing -Wno-unused-result -Wno-format-security \
                  -DSPHERE_SVR -D_CONSOLE -D_MT \
                  -I$(BASEDIR) -I$(BASEDIR)/spherelib -I$(BASEDIR)/SphereCommon \
                  -I$(BASEDIR)/SphereAccount -I$(BASEDIR)/SphereSvr \
                  -Werror=return-type
DEFAULT_CXXFLAGS = -g -m32 $(COMMON_CXXFLAGS)
DEFAULT_LDFLAGS = -m32 -lpthread

CXXFLAGS ?= $(DEFAULT_CXXFLAGS)
LDFLAGS ?= $(DEFAULT_LDFLAGS)
BUILD_DIR ?= .
TARGET ?= sphere99svr

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
	SphereCommon/cMulMap.cpp \
	SphereCommon/cmulmulti.cpp \
	SphereCommon/cMulTile.cpp \
	SphereCommon/cMulVer.cpp \
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
ifeq ($(BUILD_DIR),.)
ALL_OBJ = $(ALL_SRC:.cpp=.o)
else
ALL_OBJ = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(ALL_SRC))
endif
ALL_DEP = $(ALL_OBJ:.o=.d)

all: $(TARGET)

$(TARGET): $(ALL_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(ALL_OBJ) -o $@ $(LDFLAGS)

# -MMD -MP: track header dependencies, so editing a .h rebuilds its users
ifeq ($(BUILD_DIR),.)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@
else
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@
endif

# These targets are intentionally recursive: command-line CXXFLAGS/LDFLAGS
# replace the legacy -m32 flags while the recursive invocation reuses the same
# source list and dependency rules.  Each variant has an independent object
# tree, so target switching can never reuse incompatible .o files.
debug:
	$(MAKE) BUILD_DIR=build/debug TARGET=build/debug/sphere99svr \
		CXXFLAGS="$(COMMON_CXXFLAGS) -O0 -g3 -D_DEBUG -D_GLIBCXX_ASSERTIONS -fno-omit-frame-pointer -DSPHERE_DISABLE_CRASH_RECOVERY" \
		LDFLAGS="-lpthread" all

asan:
	$(MAKE) BUILD_DIR=build/asan TARGET=build/asan/sphere99svr \
		CXXFLAGS="$(COMMON_CXXFLAGS) -O1 -g -D_GLIBCXX_ASSERTIONS -fsanitize=address,undefined -fno-omit-frame-pointer -DSPHERE_DISABLE_CRASH_RECOVERY" \
		LDFLAGS="-fsanitize=address,undefined -lpthread" all

clean:
	rm -f $(ALL_OBJ) $(ALL_DEP) $(TARGET)
	@if [ "$(BUILD_DIR)" = "." ]; then rm -rf build; fi

-include $(ALL_DEP)

.PHONY: all debug asan clean
