#!/bin/bash

echo "Mounting /dev/sda1 to /mnt/usb..."
sudo mount /dev/sda1 /mnt/usb

echo "Copying EFI files to USB..."
sudo cp -r esp/EFI/ /mnt/usb

echo "Unmounting /dev/sda1..."
sudo umount /dev/sda1

echo "Done."   