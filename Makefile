include application.mk

SDK_ROOT := $(NMSDK)
include $(TARGET)/makedefs/common.mk
include $(TARGET)/makedefs/defs_hal.mk
include $(TARGET)/makedefs/defs_rtos.mk
include $(TARGET)/makedefs/defs_lorawan.mk
include $(TARGET)/makedefs/defs_ble.mk
include $(TARGET)/makedefs/includes_hal.mk
include $(TARGET)/makedefs/includes_rtos.mk
include $(TARGET)/makedefs/includes_lorawan.mk
include $(TARGET)/makedefs/includes_ble.mk

BSP_GENERATOR := ./tools/bsp_generator/pinconfig.py

BSP_H := $(BSP_DIR)/am_bsp_pins.h
BSP_C := $(BSP_DIR)/am_bsp_pins.c

INCLUDES += -I$(BSP_DIR)
VPATH    += $(BSP_DIR)
SRC += am_bsp_pins.c

INCLUDES += -I./bsp
VPATH    += ./bsp
SRC += am_bsp.c

CFLAGS_DBG += $(INCLUDES) $(HAL_INC) $(RTOS_INC) $(LORAWAN_INC) $(BLE_INC)
CFLAGS_REL += $(INCLUDES) $(HAL_INC) $(RTOS_INC) $(LORAWAN_INC) $(BLE_INC)

LIBS_DBG += $(HAL_LIB_DBG)
LIBS_DBG += $(RTOS_LIB_DBG)
LIBS_DBG += $(LORAWAN_LIB_DBG)
LIBS_DBG += $(BLE_LIB_DBG)
SDK_LIBS_DBG = $(LIBS_DBG:%.a=$(TARGET)/lib/%.a)

LFLAGS_DBG += $(LFLAGS)
LFLAGS_DBG += -Wl,--start-group
LFLAGS_DBG += -L$(TARGET)/lib
LFLAGS_DBG += -lm
LFLAGS_DBG += -lc
LFLAGS_DBG += -lgcc
LFLAGS_DBG += $(patsubst lib%.a,-l%,$(subst :, , $(LIBS_DBG)))
LFLAGS_DBG += --specs=nano.specs
LFLAGS_DBG += --specs=nosys.specs
LFLAGS_DBG += -Wl,--end-group
LFLAGS_DBG += -Wl,--gc-sections


LIBS_REL += $(HAL_LIB_REL)
LIBS_REL += $(RTOS_LIB_REL)
LIBS_REL += $(LORAWAN_LIB_REL)
LIBS_REL += $(BLE_LIB_REL)
SDK_LIBS_REL = $(LIBS_REL:%.a=$(TARGET)/lib/%.a)

LFLAGS_REL += $(LFLAGS)
LFLAGS_REL += -Wl,--start-group
LFLAGS_REL += -L$(TARGET)/lib
LFLAGS_REL += -lm
LFLAGS_REL += -lc
LFLAGS_REL += -lgcc
LFLAGS_REL += $(patsubst lib%.a,-l%,$(subst :, , $(LIBS_REL)))
LFLAGS_REL += --specs=nano.specs
LFLAGS_REL += --specs=nosys.specs
LFLAGS_REL += -Wl,--end-group
LFLAGS_REL += -Wl,--gc-sections

SDK_CONFIGS += FREERTOS_CONFIG=$(FREERTOS_CONFIG)
SDK_CONFIGS += LORAWAN_CONFIG=$(LORAWAN_CONFIG)
SDK_CONFIGS += BLE_CONFIG=$(BLE_CONFIG)

all: debug release

nmsdk:
	make -C $(TARGET) install $(SDK_CONFIGS)

bsp: $(BSP_H) $(BSP_C)

$(BSP_H): $(BSP_SRC)
	$(PYTHON) $(BSP_GENERATOR) $< h > $@

$(BSP_C): $(BSP_SRC)
	$(PYTHON) $(BSP_GENERATOR) $< c > $@

OBJS_DBG += $(SRC:%.c=$(BUILDDIR_DBG)/%.o)
DEPS_DBG += $(SRC:%.c=$(BUILDDIR_DBG)/%.o)
OUTPUT_DBG := $(BUILDDIR_DBG)/$(OUTPUT)$(SUFFIX_DBG).axf
OUTPUT_BIN_DBG  := $(OUTPUT_DBG:%.axf=%.bin)
OUTPUT_WIRE_DBG := $(OUTPUT_DBG:%.axf=%-wire.bin)
OUTPUT_OTA_DBG  := $(OUTPUT_DBG:%.axf=%-fuota.bin)
OUTPUT_OTA_DBG  := $(OUTPUT_DBG:%.axf=%-ota.bin)
OUTPUT_LST_DBG  := $(OUTPUT_DBG:%.axf=%.lst)
OUTPUT_SIZE_DBG := $(OUTPUT_DBG:%.axf=%.size)

debug: nmsdk bsp $(BUILDDIR_DBG) $(OUTPUT_BIN_DBG)

$(BUILDDIR_DBG):
	$(MKDIR) -p "$@"

$(OUTPUT_BIN_DBG): $(OUTPUT_DBG)
	$(OCP) $(OCPFLAGS) $< $@
	$(OD)  $(ODFLAGS) $< > $(OUTPUT_LST_DBG)
	$(SIZE) $(OBJS_DBG) $(OUTPUT_DBG)

$(OUTPUT_DBG): $(OBJS_DBG) $(SDK_LIBS_DBG)
	$(CC) -Wl,-T,$(LDSCRIPT) -o $@ $(OBJS_DBG) $(LFLAGS_DBG)

$(OUTPUT_WIRE_DBG): $(OUTPUT_BIN_DBG)
	$(PYTHON) ./tools/create_cust_image_blob.py --bin $< --load-address 0xc000 --magic-num 0xcb -o $(BUILDDIR_DBG)/temp --version 0x0
	$(PYTHON) ./tools/create_cust_wireupdate_blob.py --load-address $(UPDATE_STORAGE_ADDRESS) --bin $(BUILDDIR_DBG)/temp.bin -i 6 -o $(basename $(OUTPUT_WIRE_DBG)) --options 0x1

$(OBJS_DBG): $(BUILDDIR_DBG)/%.o : %.c
	$(CC) -c $(CFLAGS_DBG) $< -o $@

OBJS_REL += $(SRC:%.c=$(BUILDDIR_REL)/%.o)
DEPS_REL += $(SRC:%.c=$(BUILDDIR_REL)/%.o)
OUTPUT_REL := $(BUILDDIR_REL)/$(OUTPUT)$(SUFFIX_REL).axf
OUTPUT_BIN_REL  := $(OUTPUT_REL:%.axf=%.bin)
OUTPUT_WIRE_REL := $(OUTPUT_REL:%.axf=%-wire.bin)
OUTPUT_FUOTA_REL := $(OUTPUT_REL:%.axf=%-fuota.bin)
OUTPUT_OTA_REL  := $(OUTPUT_REL:%.axf=%-ota.bin)
OUTPUT_LST_REL  := $(OUTPUT_REL:%.axf=%.lst)
OUTPUT_SIZE_REL := $(OUTPUT_REL:%.axf=%.size)

release: nmsdk bsp $(BUILDDIR_REL) $(OUTPUT_BIN_REL)

$(BUILDDIR_REL):
	$(MKDIR) -p "$@"

$(OUTPUT_BIN_REL): $(OUTPUT_REL)
	$(OCP) $(OCPFLAGS) $< $@
	$(OD)  $(ODFLAGS) $< > $(OUTPUT_LST_REL)
	$(SIZE) $(OBJS_REL) $(OUTPUT_REL)

$(OUTPUT_REL): $(OBJS_REL) $(SDK_LIBS_REL)
	$(CC) -Wl,-T,$(LDSCRIPT) -o $@ $(OBJS_REL) $(LFLAGS_REL)

$(OUTPUT_FUOTA_REL): $(OUTPUT_BIN_REL)
	$(PYTHON) ./tools/create_cust_image_blob.py --bin $< --load-address 0xc000 --magic-num 0xcb -o $(basename $(OUTPUT_FUOTA_REL)) --version 0x0

$(OUTPUT_WIRE_REL): $(OUTPUT_FUOTA_REL)
	$(PYTHON) ./tools/create_cust_wireupdate_blob.py --load-address $(UPDATE_STORAGE_ADDRESS) --bin $< -i 6 -o $(basename $(OUTPUT_WIRE_REL)) --options 0x1

$(OBJS_REL): $(BUILDDIR_REL)/%.o : %.c
	$(CC) -c $(CFLAGS_REL) $< -o $@

fuota: $(OUTPUT_FUOTA_REL)

wire: $(OUTPUT_WIRE_REL)

clean-sdk:
	make -C $(TARGET) uninstall
	make -C $(TARGET) clean

clean-debug:
	$(RM) -rf $(BUILDDIR_DBG) $(BSP_H) $(BSP_C)

clean-release:
	$(RM) -rf $(BUILDDIR_REL) $(BSP_H) $(BSP_C)

clean:
	$(RM) -rf ./build $(BSP_H) $(BSP_C)

cleanall: clean clean-sdk

.phony: nmsdk