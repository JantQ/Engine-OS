CC = x86_64-w64-mingw32-gcc
CXX = x86_64-w64-mingw32-g++
CFLAGS = -ffreestanding -fshort-wchar -mno-red-zone
CXXFLAGS = -ffreestanding -fshort-wchar -mno-red-zone -fno-exceptions -fno-rtti -Wall -Wextra -O2 -Wshadow
LDFLAGS = -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e init_kernel
RT_LDFLAGS = -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e init_runtime

OVMF_CODE = /usr/share/edk2/x64/OVMF_CODE.4m.fd
OVMF_VARS = /usr/share/edk2/x64/OVMF_VARS.4m.fd

BUILD_DIR = build

C_SRC = $(shell find src -path src/Runtime -prune -o -name '*.c' -print)
CC_SRC = $(shell find src -path src/Runtime -prune -o -name '*.cc' -print)
HEADERS = $(shell find src -name '*.h') $(shell find src -name '*.hh')

RT_SRC = $(shell find src/Runtime src/Script src/Graphics src/Clock src/input src/Memory -name '*.cc') src/Text.cc

C_OBJ = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(C_SRC))
CC_OBJ = $(patsubst src/%.cc, $(BUILD_DIR)/%.o, $(CC_SRC))
OBJ = $(C_OBJ) $(CC_OBJ)

RT_OBJ = $(patsubst src/%.cc, $(BUILD_DIR)/%.o, $(RT_SRC))

OBJCOPY = x86_64-w64-mingw32-objcopy

BOOTX64.EFI: $(OBJ) $(BUILD_DIR)/RuntimeBlob.o
	$(CXX) $(LDFLAGS) -o $@ $(OBJ) $(BUILD_DIR)/RuntimeBlob.o

RUNTIME.EFI: $(RT_OBJ)
	$(CXX) $(RT_LDFLAGS) -o $@ $(RT_OBJ)

$(BUILD_DIR)/RuntimeBlob.o: RUNTIME.EFI
	@mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O pe-x86-64 -B i386:x86-64 RUNTIME.EFI $@

runtime: RUNTIME.EFI

$(BUILD_DIR)/%.o: src/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: src/%.cc $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: BOOTX64.EFI
	mkdir -p esp/EFI/BOOT
	cp BOOTX64.EFI esp/EFI/BOOT/
	[ -f OVMF_VARS.fd ] || cp $(OVMF_VARS) ./OVMF_VARS.fd
	[ -f disk.img ] || qemu-img create -f raw disk.img 64M

	qemu-system-x86_64 \
		-m 1G \
		-cpu max \
		-drive if=pflash,format=raw,unit=0,file=$(OVMF_CODE),readonly=on \
		-drive if=pflash,format=raw,unit=1,file=OVMF_VARS.fd \
		-drive format=raw,file=fat:rw:esp \
		-serial stdio \
		-drive file=disk.img,if=none,id=nvm,format=raw \
		-device nvme,serial=deadbeed,drive=nvm \

disk:
	rm -f disk.img
	qemu-img create -f raw disk.img 1G
	echo 'label: gpt' | sfdisk disk.img

clean:
	rm -rf BOOTX64.EFI RUNTIME.EFI esp OVMF_VARS.fd $(BUILD_DIR)