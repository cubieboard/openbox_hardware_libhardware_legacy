# Copyright 2006 The Android Open Source Project

LOCAL_SRC_FILES += power/power.c

ifeq ($(QEMU_HARDWARE),true)
  LOCAL_SRC_FILES += power/power_qemu.c
  LOCAL_CFLAGS    += -DQEMU_POWER=1
endif


# product is "apollo-mele"
ifeq ($(PRODUCT_CODE), apollo-mele)
  LOCAL_CFLAGS += -DPRODUCT_IS_APOLLO_MELE
endif
