################################################################################
#
# yabi
#
################################################################################

YABI_VERSION = c164926a636597da99d4bfef5e280a1f41bd275b
YABI_SITE = $(call github,angeloc,yabi,$(YABI_VERSION))
YABI_LICENSE = GPL-2.0-or-later
YABI_LICENSE_FILES = LICENSE
YABI_CONF_OPTS = \
	-DYABI_THEME_DIR=/usr/share/yabi/themes \
	-DYABI_HARDENING=OFF \
	-DYABI_SANITIZE=OFF \
	-DBUILD_TESTING=OFF

ifeq ($(BR2_INIT_BUSYBOX)$(BR2_INIT_SYSV),y)
YABI_CONF_OPTS += -DYABI_SYSVINIT=ON
else
YABI_CONF_OPTS += -DYABI_SYSVINIT=OFF
endif

ifeq ($(BR2_INIT_SYSTEMD),y)
YABI_DEPENDENCIES += systemd
YABI_CONF_OPTS += -DYABI_SYSTEMD=ON
else
YABI_CONF_OPTS += -DYABI_SYSTEMD=OFF
endif

ifeq ($(BR2_PACKAGE_YABI_DEMO),y)
define YABI_INSTALL_DEMO
	$(INSTALL) -D -m 0755 $(@D)/demo/rootfs/etc/init.d/S99app \
		$(TARGET_DIR)/etc/init.d/S99app
	$(INSTALL) -D -m 0644 $(@D)/demo/rootfs/usr/share/demo-app/app.gif \
		$(TARGET_DIR)/usr/share/demo-app/app.gif
endef
YABI_POST_INSTALL_TARGET_HOOKS += YABI_INSTALL_DEMO
endif

$(eval $(cmake-package))
