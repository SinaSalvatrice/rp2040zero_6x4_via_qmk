VIA_ENABLE = yes
TAP_DANCE_ENABLE = yes
SRC += tap_dance_override.c
CFLAGS += -include $(KEYMAP_PATH)/tap_dance_override.h
