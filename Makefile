CC = x86_64-w64-mingw32-gcc
CXX = x86_64-w64-mingw32-g++
CFLAGS = -ffreestanding -fshort-wchar -mno-red-zone
CXXFLAGS = -ffreestanding -fshort-wchar -mno-red-zone -fno-exceptions -fno-rtti
LDFLAGS = -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e init_kernel

OVMF_CODE = /usr/share/edk2/x64/OVMF_CODE.4m.fd
OVMF_VARS = /usr/share/edk2/x64/OVMF_VARS.4m.fd

C_SRC = $(wildcard src/*.c)
CC_SRC = $(wildcard src/*.cc)
HEADERS = $(wildcard src/*.h) $(wildcard src/*.hh)

C_OBJ = $(C_SRC:.c=.o)
CC_OBJ = $(CC_SRC:.cc=.o)
OBJ = $(C_OBJ) $(CC_OBJ)

BOOTX64.EFI: $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(OBJ)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cc $(HEADERS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: BOOTX64.EFI
	mkdir -p esp/EFI/BOOT
	cp BOOTX64.EFI esp/EFI/BOOT/
	cp $(OVMF_VARS) ./OVMF_VARS.fd
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,unit=0,file=$(OVMF_CODE),readonly=on \
		-drive if=pflash,format=raw,unit=1,file=OVMF_VARS.fd \
		-drive format=raw,file=fat:rw:esp \
		-serial stdio

clean:
	rm -rf BOOTX64.EFI esp OVMF_VARS.fd $(OBJ)