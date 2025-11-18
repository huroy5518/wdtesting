include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=wifi-driver-testing
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
SRC_DIR=src

KERNEL_INCLUDE_OPT:=-I$(LINUX_DIR)/include -I$(LINUX_DIR)/user_headers/include

include $(INCLUDE_DIR)/package.mk

define Package/wifi-driver-testing
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=Try to test wifi-driver
	DEPENDS:=
	MENU:=1
endef

define Package/wif-driver-testing/description
	This is a wifi-driver testing framework
endef

define Package/wifi-driver-testing/config
        source "$(SOURCE)/Config.in"
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	cp -R ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		KERNEL_INCLUDE_OPT="$(KERNEL_INCLUDE_OPT)" \
		MAKE="$(MAKE)"
endef

define Package/wifi-driver-testing/install
	echo "No Install"
endef

$(eval $(call BuildPackage,wifi-driver-testing))
