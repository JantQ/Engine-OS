CC = x86_64-w64-mingw32-gcc
CXX = x86_64-w64-mingw32-g++
CFLAGS = -ffreestanding -fshort-wchar -mno-red-zone
CXXFLAGS = -ffreestanding -fshort-wchar -mno-red-zone -fno-exceptions -fno-rtti
LDFLAGS = -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e init_kernel

OVMF_CODE = /usr/share/edk2/x64/OVMF_CODE.4m.fd
OVMF_VARS = /usr/share/edk2/x64/OVMF_VARS.4m.fd

BUILD_DIR = build

C_SRC = $(shell find src -name '*.c')
CC_SRC = $(shell find src -name '*.cc')
HEADERS = $(shell find src -name '*.h') $(shell find src -name '*.hh')

C_OBJ = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(C_SRC))
CC_OBJ = $(patsubst src/%.cc, $(BUILD_DIR)/%.o, $(CC_SRC))
OBJ = $(C_OBJ) $(CC_OBJ)

BOOTX64.EFI: $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(OBJ)

$(BUILD_DIR)/%.o: src/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: src/%.cc $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: BOOTX64.EFI
	mkdir -p esp/EFI/BOOT
	cp BOOTX64.EFI esp/EFI/BOOT/
	cp $(OVMF_VARS) ./OVMF_VARS.fd
	qemu-system-x86_64 \
		-m 1G \
		-drive if=pflash,format=raw,unit=0,file=$(OVMF_CODE),readonly=on \
		-drive if=pflash,format=raw,unit=1,file=OVMF_VARS.fd \
		-drive format=raw,file=fat:rw:esp \
		-serial stdio

clean:
	rm -rf BOOTX64.EFI esp OVMF_VARS.fd $(BUILD_DIR)