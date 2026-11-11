################################################################################
#
# fbv-ng
#
################################################################################

FBV_NG_VERSION = 0.1.0
FBV_NG_SITE = $(call github,angeloc,fbv-ng,$(FBV_NG_VERSION))
FBV_NG_LICENSE = GPL-2.0-or-later
FBV_NG_LICENSE_FILES = LICENSE
FBV_NG_CONF_OPTS = \
	-DFBVNG_HARDENING=OFF \
	-DFBVNG_SANITIZE=OFF \
	-DFBVNG_FUZZ=OFF \
	-DBUILD_TESTING=OFF

ifeq ($(BR2_PACKAGE_LIBPNG),y)
FBV_NG_DEPENDENCIES += libpng
FBV_NG_CONF_OPTS += -DFBVNG_WITH_PNG=ON
else
FBV_NG_CONF_OPTS += -DFBVNG_WITH_PNG=OFF
endif

ifeq ($(BR2_PACKAGE_JPEG),y)
FBV_NG_DEPENDENCIES += jpeg
FBV_NG_CONF_OPTS += -DFBVNG_WITH_JPEG=ON
else
FBV_NG_CONF_OPTS += -DFBVNG_WITH_JPEG=OFF
endif

ifeq ($(BR2_PACKAGE_GIFLIB),y)
FBV_NG_DEPENDENCIES += giflib
FBV_NG_CONF_OPTS += -DFBVNG_WITH_GIF=ON
else
FBV_NG_CONF_OPTS += -DFBVNG_WITH_GIF=OFF
endif

$(eval $(cmake-package))
